#include "pch_game.h"
#include "CardGame.h"

#include <chrono>
#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>
#include "Cards/ArtifactCard.h"
#include "Cards/SpellCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/RoomCard.h"
#include "Engine/TextureStreamer.h"
#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/PixelPerfectRenderInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "Levels/GameOverLevel.h"
#include "Levels/Level.h"
#include "Levels/MainLevel.h"
#include "Levels/MainMenuLevel.h"
#include "Levels/NewGameLevel.h"
#include "Levels/PartySelectLevel.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include <time.h>

#include "JLib/Math.h"
#include "RenderGraph/RenderGraph.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

	constexpr glm::ivec2 RESOLUTION = SIMULATED_RESOLUTION * 3;

	struct KeyCallback final
	{
		uint32_t key;
		uint32_t action;
	};

	struct CardGame final
	{
		struct FrameBuffer final
		{
			jv::ge::Resource image;
			jv::ge::Resource frameBuffer;
			jv::ge::Resource semaphore;
			jv::ge::Resource sampler;
		};

		struct SwapChainPushConstant
		{
			glm::ivec2 resolution;
			glm::ivec2 simResolution;
			float time;
			float pixelation;
		};

		struct SwapChain final
		{
			jv::Array<FrameBuffer> frameBuffers;
			jv::ge::Resource pool;
			jv::ge::Resource renderPass;
			jv::ge::Resource shader;
			jv::ge::Resource layout;
			jv::ge::Resource pipeline;
		};

		LevelUpdateInfo::ScreenShakeInfo screenShakeInfo{};

		Engine engine;
		jv::Arena arena;
		jv::Arena levelArena;
		jv::ge::Resource scene;
		jv::ge::Resource levelScene;
		jv::ge::Resource atlas;
		InputState inputState{};

		SwapChain swapChain;
		float timeSinceStarted;

		jv::Array<jv::ge::AtlasTexture> atlasTextures;
		jv::Array<glm::ivec2> subTextureResolutions;
		TaskSystem<RenderTask>* renderTasks;
		TaskSystem<DynamicRenderTask>* dynamicRenderTasks;
		TaskSystem<RenderTask>* priorityRenderTasks;
		TaskSystem<DynamicRenderTask>* dynamicPriorityRenderTasks;
		TaskSystem<TextTask>* textTasks;
		TaskSystem<PixelPerfectRenderTask>* pixelPerfectRenderTasks;
		TaskSystem<LightTask>* lightTasks;
		InstancedRenderInterpreter<RenderTask>* renderInterpreter;
		InstancedRenderInterpreter<RenderTask>* priorityRenderInterpreter;
		DynamicRenderInterpreter* dynamicRenderInterpreter;
		DynamicRenderInterpreter* dynamicPriorityRenderInterpreter;
		TextInterpreter* textInterpreter;
		PixelPerfectRenderInterpreter* pixelPerfectRenderInterpreter;

		GameState gameState{};
		PlayerState playerState{};

		LevelIndex levelIndex = LevelIndex::mainMenu;
		bool levelLoading = true;
		uint32_t levelLoadingFrame = 0;

		jv::Array<Level*> levels{};

		jv::LinkedList<KeyCallback> keyCallbacks{};
		jv::LinkedList<KeyCallback> mouseCallbacks{};
		float scrollCallback = 0;

		jv::Array<MonsterCard> monsters;
		jv::Array<ArtifactCard> artifacts;
		jv::Array<uint32_t> bosses;
		jv::Array<RoomCard> rooms;
		jv::Array<SpellCard> spells;
		jv::Array<CurseCard> curses;
		jv::Array<EventCard> events;

		std::chrono::high_resolution_clock timer{};
		std::chrono::time_point<std::chrono::steady_clock> prevTime{};

		TextureStreamer textureStreamer;
		float pixelation = 1;

		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(const CardGame& cardGame);

		[[nodiscard]] static jv::Array<const char*> GetTexturePaths(jv::Arena& arena);
		[[nodiscard]] static jv::Array<const char*> GetDynamicTexturePaths(jv::Arena& arena, jv::Arena& frameArena);
		[[nodiscard]] static jv::Array<MonsterCard> GetMonsterCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<ArtifactCard> GetArtifactCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<uint32_t> GetBossCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<RoomCard> GetRoomCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<SpellCard> GetSpellCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<CurseCard> GetCurseCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<EventCard> GetEventCards(jv::Arena& arena);
		
		void UpdateInput();
		static void SetInputState(InputState::State& state, uint32_t target, KeyCallback callback);
		static void OnKeyCallback(size_t key, size_t action);
		static void OnMouseCallback(size_t key, size_t action);
		static void OnScrollCallback(glm::vec<2, double> offset);
	} cardGame{};

	bool cardGameRunning = false;

	bool CardGame::Update()
	{
		UpdateInput();
		textureStreamer.Update();

		if(levelLoading)
		{
			if (++levelLoadingFrame < jv::ge::GetFrameCount())
				return true;
			levelLoadingFrame = 0;

			jv::ge::ClearScene(levelScene);
			levelArena.Clear();
			const LevelCreateInfo info
			{
				levelArena,
				engine.GetMemory().tempArena,
				engine.GetMemory().frameArena,
				levelScene,
				atlasTextures,
				gameState,
				playerState,
				monsters,
				artifacts,
				bosses,
				rooms,
				spells,
				curses,
				events
			};
			
			levels[static_cast<uint32_t>(levelIndex)]->Create(info);
			levelLoading = false;
		}

		const auto currentTime = timer.now();
		const auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - prevTime).count();

		const float dt = static_cast<float>(deltaTime) / 1e3f;
		timeSinceStarted += dt;
		
		if (screenShakeInfo.IsInTimeOut())
			screenShakeInfo.remaining -= dt;
		
		const LevelUpdateInfo info
		{
			levelArena,
			engine.GetMemory().tempArena,
			engine.GetMemory().frameArena,
			levelScene,
			atlasTextures,
			gameState,
			playerState,
			monsters,
			artifacts,
			bosses,
			rooms,
			spells,
			curses,
			events,
			RESOLUTION,
			inputState,
			*textTasks,
			*pixelPerfectRenderTasks,
			*lightTasks,
			dt,
			textureStreamer,
			screenShakeInfo,
			pixelation
		};

		const bool waitForImage = jv::ge::WaitForImage();
		if (!waitForImage)
			return false;

		prevTime = currentTime;
		auto loadLevelIndex = levelIndex;
		const auto level = levels[static_cast<uint32_t>(levelIndex)];
		auto result = level->Update(info, loadLevelIndex);
		if (!result)
			return result;
		level->PostUpdate(info);

		if(loadLevelIndex != levelIndex)
		{
			levelIndex = loadLevelIndex;
			levelLoading = true;
		}

		result = engine.Update([](void* userPtr)
		{
			const auto cardGame = static_cast<CardGame*>(userPtr);
			const auto& swapChain = cardGame->swapChain;
			const uint32_t frameIndex = jv::ge::GetFrameIndex();
			auto& frameBuffer = swapChain.frameBuffers[frameIndex];

			jv::ge::RenderFrameInfo renderFrameInfo{};
			renderFrameInfo.frameBuffer = frameBuffer.frameBuffer;
			renderFrameInfo.signalSemaphore = frameBuffer.semaphore;
			if (!RenderFrame(renderFrameInfo))
				return false;

			jv::ge::WriteInfo::Binding writeBindingInfo{};
			writeBindingInfo.type = jv::ge::BindingType::sampler;
			writeBindingInfo.image.image = frameBuffer.image;
			writeBindingInfo.image.sampler = frameBuffer.sampler;
			writeBindingInfo.index = 0;

			jv::ge::WriteInfo writeInfo{};
			writeInfo.descriptorSet = jv::ge::GetDescriptorSet(swapChain.pool, frameIndex);
			writeInfo.bindings = &writeBindingInfo;
			writeInfo.bindingCount = 1;
			Write(writeInfo);
			
			SwapChainPushConstant pushConstant{};
			pushConstant.time = cardGame->timeSinceStarted;
			pushConstant.resolution = RESOLUTION;
			pushConstant.simResolution = SIMULATED_RESOLUTION;
			pushConstant.pixelation = cardGame->pixelation;

			jv::ge::DrawInfo drawInfo{};
			drawInfo.pipeline = swapChain.pipeline;
			drawInfo.mesh = cardGame->dynamicRenderInterpreter->GetFallbackMesh();
			drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(swapChain.pool, frameIndex);
			drawInfo.descriptorSetCount = 1;
			drawInfo.pushConstantSize = sizeof(SwapChainPushConstant);
			drawInfo.pushConstant = &pushConstant;
			drawInfo.pushConstantStage = jv::ge::ShaderStage::fragment;
			Draw(drawInfo);

			jv::ge::RenderFrameInfo finalRenderFrameInfo{};
			finalRenderFrameInfo.waitSemaphores = &frameBuffer.semaphore;
			finalRenderFrameInfo.waitSemaphoreCount = 1;
			if (!RenderFrame(finalRenderFrameInfo))
				return false;
			return true;
		}, this);
		return result;
	}

	void CardGame::Create(CardGame* outCardGame)
	{
		srand(time(nullptr));

		*outCardGame = {};
		{
			EngineCreateInfo engineCreateInfo{};
			engineCreateInfo.onKeyCallback = OnKeyCallback;
			engineCreateInfo.onMouseCallback = OnMouseCallback;
			engineCreateInfo.onScrollCallback = OnScrollCallback;
			engineCreateInfo.resolution = RESOLUTION;
			outCardGame->engine = Engine::Create(engineCreateInfo);
		}

		outCardGame->arena = outCardGame->engine.CreateSubArena(1024);
		outCardGame->levelArena = outCardGame->engine.CreateSubArena(1024);
		outCardGame->scene = jv::ge::CreateScene();
		outCardGame->levelScene = jv::ge::CreateScene();

		const auto mem = outCardGame->engine.GetMemory();

#ifdef _DEBUG
		{
			const auto tempScope = mem.tempArena.CreateScope();

			const auto paths = cardGame.GetTexturePaths(mem.tempArena);
			jv::ge::GenerateAtlas(outCardGame->arena, mem.tempArena, paths,
				ATLAS_PATH, ATLAS_META_DATA_PATH);

			mem.tempArena.DestroyScope(tempScope);
		}
#endif

		// TEMP NO RENDER GRAPH
		{
			auto& swapChain = outCardGame->swapChain;

			jv::ge::RenderPassCreateInfo renderPassCreateInfo{};
			renderPassCreateInfo.target = jv::ge::RenderPassCreateInfo::DrawTarget::image;
			swapChain.renderPass = CreateRenderPass(renderPassCreateInfo);

			const uint32_t frameCount = jv::ge::GetFrameCount();
			swapChain.frameBuffers = jv::CreateArray<FrameBuffer>(mem.arena, frameCount);

			for (uint32_t i = 0; i < frameCount; ++i)
			{
				auto& frameBuffer = swapChain.frameBuffers[i];

				jv::ge::ImageCreateInfo imageCreateInfo{};
				imageCreateInfo.resolution = SIMULATED_RESOLUTION;
				imageCreateInfo.scene = outCardGame->scene;
				frameBuffer.image = AddImage(imageCreateInfo);

				jv::ge::FrameBufferCreateInfo frameBufferCreateInfo{};
				frameBufferCreateInfo.renderPass = swapChain.renderPass;
				frameBufferCreateInfo.images = &frameBuffer.image;
				frameBuffer.frameBuffer = CreateFrameBuffer(frameBufferCreateInfo);
				frameBuffer.semaphore = jv::ge::CreateSemaphore();

				jv::ge::SamplerCreateInfo samplerCreateInfo{};
				samplerCreateInfo.scene = outCardGame->scene;
				frameBuffer.sampler = AddSampler(samplerCreateInfo);
			}
			
			const auto vertCode = jv::file::Load(mem.frameArena, "Shaders/vert-sc.spv");
			const auto fragCode = jv::file::Load(mem.frameArena, "Shaders/frag-sc.spv");

			jv::ge::ShaderCreateInfo shaderCreateInfo{};
			shaderCreateInfo.vertexCode = vertCode.ptr;
			shaderCreateInfo.vertexCodeLength = vertCode.length;
			shaderCreateInfo.fragmentCode = fragCode.ptr;
			shaderCreateInfo.fragmentCodeLength = fragCode.length;
			swapChain.shader = CreateShader(shaderCreateInfo);

			jv::ge::LayoutCreateInfo::Binding bindingCreateInfo{};
			bindingCreateInfo.stage = jv::ge::ShaderStage::fragment;
			bindingCreateInfo.type = jv::ge::BindingType::sampler;

			jv::ge::LayoutCreateInfo layoutCreateInfo{};
			layoutCreateInfo.bindings = &bindingCreateInfo;
			layoutCreateInfo.bindingsCount = 1;
			swapChain.layout = CreateLayout(layoutCreateInfo);

			jv::ge::DescriptorPoolCreateInfo poolCreateInfo{};
			poolCreateInfo.layout = swapChain.layout;
			poolCreateInfo.capacity = frameCount;
			poolCreateInfo.scene = outCardGame->scene;
			swapChain.pool = AddDescriptorPool(poolCreateInfo);

			jv::ge::PipelineCreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.resolution = RESOLUTION;
			pipelineCreateInfo.shader = swapChain.shader;
			pipelineCreateInfo.layoutCount = 1;
			pipelineCreateInfo.layouts = &swapChain.layout;
			pipelineCreateInfo.vertexType = jv::ge::VertexType::v3D;
			pipelineCreateInfo.pushConstantSize = sizeof(SwapChainPushConstant);
			pipelineCreateInfo.pushConstantStage = jv::ge::ShaderStage::fragment;
			swapChain.pipeline = CreatePipeline(pipelineCreateInfo);
		}

		int texWidth, texHeight, texChannels2;
		{
			stbi_uc* pixels = stbi_load(ATLAS_PATH, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

			jv::ge::ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.resolution = { texWidth, texHeight };
			imageCreateInfo.scene = outCardGame->scene;
			outCardGame->atlas = AddImage(imageCreateInfo);
			jv::ge::FillImage(outCardGame->atlas, pixels);
			stbi_image_free(pixels);
			outCardGame->atlasTextures = jv::ge::LoadAtlasMetaData(outCardGame->arena, ATLAS_META_DATA_PATH);
		}

		{
			outCardGame->renderTasks = &outCardGame->engine.AddTaskSystem<RenderTask>();
			outCardGame->renderTasks->Allocate(outCardGame->arena, 1024);
			outCardGame->dynamicRenderTasks = &outCardGame->engine.AddTaskSystem<DynamicRenderTask>();
			outCardGame->dynamicRenderTasks->Allocate(outCardGame->arena, 32);
			outCardGame->priorityRenderTasks = &outCardGame->engine.AddTaskSystem<RenderTask>();
			outCardGame->priorityRenderTasks->Allocate(outCardGame->arena, 512);
			outCardGame->dynamicPriorityRenderTasks = &outCardGame->engine.AddTaskSystem<DynamicRenderTask>();
			outCardGame->dynamicPriorityRenderTasks->Allocate(outCardGame->arena, 16);
			outCardGame->textTasks = &outCardGame->engine.AddTaskSystem<TextTask>();
			outCardGame->textTasks->Allocate(outCardGame->arena, 32);
			outCardGame->pixelPerfectRenderTasks = &outCardGame->engine.AddTaskSystem<PixelPerfectRenderTask>();
			outCardGame->pixelPerfectRenderTasks->Allocate(outCardGame->arena, 128);
			outCardGame->lightTasks = &outCardGame->engine.AddTaskSystem<LightTask>();
			outCardGame->lightTasks->Allocate(outCardGame->arena, 16);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = SIMULATED_RESOLUTION;
			createInfo.drawsDirectlyToSwapChain = false;

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = outCardGame->scene;
			enableInfo.capacity = 1024;

			DynamicRenderInterpreterCreateInfo dynamicCreateInfo{};
			dynamicCreateInfo.resolution = SIMULATED_RESOLUTION;
			dynamicCreateInfo.frameArena = &mem.frameArena;
			dynamicCreateInfo.drawsDirectlyToSwapChain = false;
			dynamicCreateInfo.lightTasks = outCardGame->lightTasks;

			DynamicRenderInterpreterEnableInfo dynamicEnableInfo{};
			dynamicEnableInfo.arena = &outCardGame->arena;
			dynamicEnableInfo.scene = outCardGame->scene;
			dynamicEnableInfo.capacity = 32;

			outCardGame->dynamicPriorityRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<DynamicRenderTask, DynamicRenderInterpreter>(
				*outCardGame->dynamicPriorityRenderTasks, dynamicCreateInfo);
			outCardGame->dynamicPriorityRenderInterpreter->Enable(dynamicEnableInfo);

			outCardGame->priorityRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter<RenderTask>>(
				*outCardGame->priorityRenderTasks, createInfo);
			outCardGame->priorityRenderInterpreter->Enable(enableInfo);
			outCardGame->priorityRenderInterpreter->image = outCardGame->atlas;

			outCardGame->dynamicRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<DynamicRenderTask, DynamicRenderInterpreter>(
				*outCardGame->dynamicRenderTasks, dynamicCreateInfo);
			outCardGame->dynamicRenderInterpreter->Enable(dynamicEnableInfo);

			outCardGame->renderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter<RenderTask>>(
				*outCardGame->renderTasks, createInfo);
			outCardGame->renderInterpreter->Enable(enableInfo);
			outCardGame->renderInterpreter->image = outCardGame->atlas;

			PixelPerfectRenderInterpreterCreateInfo pixelPerfectRenderInterpreterCreateInfo{};
			pixelPerfectRenderInterpreterCreateInfo.renderTasks = outCardGame->renderTasks;
			pixelPerfectRenderInterpreterCreateInfo.priorityRenderTasks = outCardGame->priorityRenderTasks;
			pixelPerfectRenderInterpreterCreateInfo.dynRenderTasks = outCardGame->dynamicRenderTasks;
			pixelPerfectRenderInterpreterCreateInfo.dynPriorityRenderTasks = outCardGame->dynamicPriorityRenderTasks;
			pixelPerfectRenderInterpreterCreateInfo.screenShakeInfo = &outCardGame->screenShakeInfo;

			pixelPerfectRenderInterpreterCreateInfo.resolution = SIMULATED_RESOLUTION;
			pixelPerfectRenderInterpreterCreateInfo.simulatedResolution = SIMULATED_RESOLUTION;
			pixelPerfectRenderInterpreterCreateInfo.background = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;

			outCardGame->pixelPerfectRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<PixelPerfectRenderTask, PixelPerfectRenderInterpreter>(
				*outCardGame->pixelPerfectRenderTasks, pixelPerfectRenderInterpreterCreateInfo);

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.alphabetAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.symbolAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
			textInterpreterCreateInfo.fadeInSpeed = TEXT_DRAW_SPEED;

			textInterpreterCreateInfo.renderTasks = outCardGame->pixelPerfectRenderTasks;
			outCardGame->textInterpreter = &outCardGame->engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->textTasks, textInterpreterCreateInfo);
		}

		{
			outCardGame->monsters = cardGame.GetMonsterCards(outCardGame->arena);
			outCardGame->artifacts = cardGame.GetArtifactCards(outCardGame->arena);
			outCardGame->bosses = cardGame.GetBossCards(outCardGame->arena);
			outCardGame->rooms = cardGame.GetRoomCards(outCardGame->arena);
			outCardGame->spells = cardGame.GetSpellCards(outCardGame->arena);
			outCardGame->curses = cardGame.GetCurseCards(outCardGame->arena);
			outCardGame->events = cardGame.GetEventCards(outCardGame->arena);
		}

		{
			outCardGame->levels = jv::CreateArray<Level*>(outCardGame->arena, 5);
			outCardGame->levels[0] = outCardGame->arena.New<MainMenuLevel>();
			outCardGame->levels[1] = outCardGame->arena.New<NewGameLevel>();
			outCardGame->levels[2] = outCardGame->arena.New<PartySelectLevel>();
			outCardGame->levels[3] = outCardGame->arena.New<MainLevel>();
			outCardGame->levels[4] = outCardGame->arena.New<GameOverLevel>();
		}

		outCardGame->prevTime = outCardGame->timer.now();

		jv::ge::ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.resolution = CARD_ART_SHAPE * glm::ivec2(CARD_ART_LENGTH, 1);
		imageCreateInfo.scene = outCardGame->scene;

		outCardGame->textureStreamer = TextureStreamer::Create(outCardGame->arena, 32, 256, imageCreateInfo);
		const auto dynTexts = GetDynamicTexturePaths(outCardGame->engine.GetMemory().arena, outCardGame->engine.GetMemory().frameArena);
		for (const auto& dynText : dynTexts)
			outCardGame->textureStreamer.DefineTexturePath(dynText);

		// Render graph.
		/*
		{
			auto mem = outCardGame->engine.GetMemory();
			const auto resources = jv::CreateArray<jv::rg::ResourceMaskDescription>(mem.frameArena, 2);
			const auto nodes = jv::CreateArray<jv::rg::RenderGraphNodeInfo>(mem.frameArena, 3);

			// Scene.
			uint32_t outSceneImage = 0;
			nodes[0].inResourceCount = 0;
			nodes[0].outResourceCount = 1;
			nodes[0].outResources = &outSceneImage;

			// Background.
			uint32_t outBgImage = 1;
			nodes[1].inResourceCount = 0;
			nodes[1].outResourceCount = 1;
			nodes[1].outResources = &outBgImage;

			// Final node.
			uint32_t finalInResources[2]{outSceneImage, outBgImage};
			nodes[2].inResourceCount = 2;
			nodes[2].inResources = finalInResources;

			jv::rg::RenderGraphCreateInfo renderGraphCreateInfo{};
			renderGraphCreateInfo.resources = resources.ptr;
			renderGraphCreateInfo.resourceCount = resources.length;
			renderGraphCreateInfo.nodes = nodes.ptr;
			renderGraphCreateInfo.nodeCount = nodes.length;
			const auto graph = jv::rg::RenderGraph::Create(mem.arena, mem.tempArena, renderGraphCreateInfo);
		}
		*/
	}

	void CardGame::Destroy(const CardGame& cardGame)
	{
		jv::Arena::Destroy(cardGame.arena);
		Engine::Destroy(cardGame.engine);
	}

	jv::Array<const char*> CardGame::GetTexturePaths(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<const char*>(arena, static_cast<uint32_t>(TextureId::length));
		arr[0] = "Art/alphabet.png";
		arr[1] = "Art/numbers.png";
		arr[2] = "Art/symbols.png";
		arr[3] = "Art/mouse.png";
		arr[4] = "Art/card.png";
		arr[5] = "Art/card-large.png";
		arr[6] = "Art/button.png";
		arr[7] = "Art/stats.png";
		arr[8] = "Art/fallback.png";
		arr[9] = "Art/empty.png";
		arr[10] = "Art/button-small.png";
		return arr;
	}

	jv::Array<const char*> CardGame::GetDynamicTexturePaths(jv::Arena& arena, jv::Arena& frameArena)
	{
		constexpr uint32_t l = 30;
		const auto arr = jv::CreateArray<const char*>(frameArena, l * 2);
		for (uint32_t i = 0; i < l; ++i)
		{
			const char* prefix = "Art/Monsters/";
			arr[i] = arr[i + l] = TextInterpreter::Concat(prefix, TextInterpreter::IntToConstCharPtr(i + 1, frameArena), frameArena);
			arr[i + l] = TextInterpreter::Concat(arr[i], "_norm.png", arena);
			arr[i] = TextInterpreter::Concat(arr[i], ".png", arena);
		}

		return arr;
	}

	enum class TypeTarget
	{
		allies,
		enemies,
		all
	};

	void TargetOfType(const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self, const uint32_t tags, const TypeTarget target)
	{
		ActionState cpyState = actionState;

		const auto& boardState = state.boardState;
		if(target == TypeTarget::all || (self < BOARD_CAPACITY_PER_SIDE && target == TypeTarget::allies || 
			self >= BOARD_CAPACITY_PER_SIDE && target == TypeTarget::enemies))
			for (uint32_t i = 0; i < boardState.allyCount; ++i)
			{
				if (tags != -1 && (info.monsters[boardState.ids[i]].tags & tags) == 0)
					continue;
				cpyState.dst = i;
				cpyState.dstUniqueId = boardState.uniqueIds[i];
				state.stack.Add() = cpyState;
			}
		if (target == TypeTarget::all || (self >= BOARD_CAPACITY_PER_SIDE && target == TypeTarget::allies || 
			self < BOARD_CAPACITY_PER_SIDE && target == TypeTarget::enemies))
			for (uint32_t i = 0; i < boardState.enemyCount; ++i)
			{
				if (tags != -1 && (info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]].tags & tags) == 0)
					continue;
				cpyState.dst = BOARD_CAPACITY_PER_SIDE + i;
				cpyState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
				state.stack.Add() = cpyState;
			}
	}

	jv::Array<MonsterCard> CardGame::GetMonsterCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MonsterCard>(arena, MONSTER_IDS::LENGTH);

		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		auto& vulture = arr[MONSTER_IDS::VULTURE];
		vulture.name = "vulture";
		vulture.health = 1;
		vulture.attack = 1;
		vulture.tags = TAG_TOKEN;
		vulture.unique = true;
		auto& goblin = arr[MONSTER_IDS::GOBLIN];
		goblin.name = "goblin";
		goblin.health = 1;
		goblin.attack = 1;
		goblin.tags = TAG_TOKEN | TAG_GOBLIN;
		goblin.unique = true;
		auto& demon = arr[MONSTER_IDS::DEMON];
		demon.name = "demon";
		demon.health = 0;
		demon.attack = 0;
		demon.tags = TAG_TOKEN;
		demon.unique = true;
		auto& elf = arr[MONSTER_IDS::ELF];
		elf.name = "elf";
		elf.health = 1;
		elf.attack = 0;
		elf.tags = TAG_TOKEN | TAG_ELF;
		elf.unique = true;
		elf.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if(actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					++state.mana;
					return true;
				}
				return false;
			};
		auto& treasure = arr[MONSTER_IDS::TREASURE];
		treasure.name = "treasure";
		treasure.health = 1;
		treasure.attack = 0;
		treasure.tags = TAG_TOKEN;
		treasure.unique = true;
		treasure.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (actionState.dst != self || actionState.dstUniqueId != state.boardState.uniqueIds[self])
						return false;
					++state.mana;
					return true;
				}
				return false;
			};
		auto& daisy = arr[MONSTER_IDS::DAISY];
		daisy.name = "daisy";
		daisy.health = 6;
		daisy.attack = 4;
		daisy.unique = true;
		daisy.ruleText = "loves you unconditionally.";
		auto& god = arr[MONSTER_IDS::GOD];
		god.name = "god";
		god.health = 99;
		god.attack = 99;
		god.unique = true;
		auto& ogre = arr[MONSTER_IDS::OGRE];
		ogre.name = "ogre";
		ogre.attack = 2;
		ogre.health = 20;
		auto& wolf = arr[MONSTER_IDS::WOLF];
		wolf.name = "wolf";
		wolf.attack = 3;
		wolf.health = 14;
		auto& spirit = arr[MONSTER_IDS::SPIRIT];
		spirit.name = "spirit";
		spirit.attack = 7;
		spirit.health = 1;
		auto& dwarf = arr[MONSTER_IDS::DWARF];
		dwarf.name = "dwarf";
		dwarf.attack = 1;
		dwarf.health = 26;
		auto& gorilla = arr[MONSTER_IDS::GORILLA];
		gorilla.name = "gorilla";
		gorilla.attack = 4;
		gorilla.health = 11;
		auto& fairy = arr[MONSTER_IDS::FAIRY];
		fairy.name = "fairy";
		fairy.attack = 5;
		fairy.health = 5;
		auto& bob = arr[MONSTER_IDS::BOB];
		bob.name = "bob";
		bob.attack = 1;
		bob.health = 1;
		bob.ruleText = "bob.";
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, ARTIFACT_IDS::LENGTH);

		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].name = "amulet of arcane acuity";
		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].ruleText = "[start of turn] gain one mana.";
		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
			{
				++state.mana;
				return true;
			}
			return false;
		};
		arr[ARTIFACT_IDS::THORNMAIL].name = "thornmail";
		arr[ARTIFACT_IDS::THORNMAIL].ruleText = "[attacked] deal 1 damage to the attacker.";
		arr[ARTIFACT_IDS::THORNMAIL].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if (actionState.dst != self || actionState.dstUniqueId != boardState.uniqueIds[self])
						return false;

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::board;
					damageState.src = self;
					damageState.srcUniqueId = boardState.uniqueIds[self];
					damageState.dst = actionState.src;
					damageState.dstUniqueId = actionState.srcUniqueId;
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MAGE_ARMOR].name = "mage armor";
		arr[ARTIFACT_IDS::MAGE_ARMOR].ruleText = "[cast] gain 2 temporary health.";
		arr[ARTIFACT_IDS::MAGE_ARMOR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& boardState = state.boardState;
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].name = "mask of eternal youth";
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].ruleText = "[enemy death] gain 1 health.";
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if(actionState.dst >= BOARD_CAPACITY_PER_SIDE)
					{
						const auto& boardState = state.boardState;
						ActionState buffState{};
						buffState.trigger = ActionState::Trigger::onStatBuff;
						buffState.source = ActionState::Source::board;
						buffState.src = self;
						buffState.srcUniqueId = boardState.uniqueIds[self];
						buffState.dst = buffState.src;
						buffState.dstUniqueId = buffState.srcUniqueId;
						buffState.values[ActionState::VStatBuff::health] = 1;
						state.stack.Add() = buffState;
						return true;
					}
				}
				return false;
			};
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].name = "corrupting knife";
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].ruleText = "[end of turn] gain 2 attack and take one damage.";
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::attack] = 2;
					state.stack.Add() = buffState;

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::board;
					damageState.src = self;
					damageState.srcUniqueId = boardState.uniqueIds[self];
					damageState.dst = buffState.src;
					damageState.dstUniqueId = buffState.srcUniqueId;
					damageState.values[ActionState::VDamage::damage] = 1;
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].name = "sacrificial altar";
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].ruleText = "[start of turn] die to give all allies +0/x where x is your health.";
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					const auto& boardState = state.boardState;

					ActionState deathState{};
					deathState.trigger = ActionState::Trigger::onDeath;
					deathState.source = ActionState::Source::board;
					deathState.src = self;
					deathState.srcUniqueId = boardState.uniqueIds[self];
					deathState.dst = deathState.src;
					deathState.dstUniqueId = deathState.srcUniqueId;
					state.stack.Add() = deathState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::health] = boardState.combatStats[self].health;
					TargetOfType(info, state, buffState, self, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLOOD_AXE].name = "blood axe";
		arr[ARTIFACT_IDS::BLOOD_AXE].ruleText = "[kill] gain 3 health.";
		arr[ARTIFACT_IDS::BLOOD_AXE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;

					if (actionState.source != ActionState::Source::board)
						return false;
					if (actionState.src != self || actionState.srcUniqueId != boardState.uniqueIds[self])
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::health] = 3;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_CLAW].name = "rusty claw";
		arr[ARTIFACT_IDS::RUSTY_CLAW].ruleText = "[kill] draw 3 cards.";
		arr[ARTIFACT_IDS::RUSTY_CLAW].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;
					if (actionState.source != ActionState::Source::board)
						return false;
					if (actionState.src != self || actionState.srcUniqueId != boardState.uniqueIds[self])
						return false;

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::board;
					drawState.src = self;
					drawState.srcUniqueId = boardState.uniqueIds[self];
					for (uint32_t i = 0; i < 3; ++i)
						state.stack.Add() = drawState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLOOD_HAMMER].name = "blood hammer";
		arr[ARTIFACT_IDS::BLOOD_HAMMER].ruleText = "[any death] gain 3 temporary attack.";
		arr[ARTIFACT_IDS::BLOOD_HAMMER].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::tempAttack] = 3;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::SPIKEY_COLLAR].name = "spikey collar";
		arr[ARTIFACT_IDS::SPIKEY_COLLAR].ruleText = "[damaged] untap.";
		arr[ARTIFACT_IDS::SPIKEY_COLLAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					const auto& boardState = state.boardState;
					if (actionState.dst != self || actionState.dstUniqueId != boardState.uniqueIds[self])
						return false;

					state.tapped[self] = false;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLESSED_RING].name = "blessed ring";
		arr[ARTIFACT_IDS::BLESSED_RING].ruleText = "[summoned] gain +0/3.";
		arr[ARTIFACT_IDS::BLESSED_RING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onSummon)
				{
					const auto& boardState = state.boardState;
					const bool isAlly = actionState.values[ActionState::VSummon::isAlly];
					if (!isAlly)
						return false;

					if(self == boardState.allyCount - 1)
					{
						ActionState buffState{};
						buffState.trigger = ActionState::Trigger::onStatBuff;
						buffState.source = ActionState::Source::board;
						buffState.src = self;
						buffState.srcUniqueId = boardState.uniqueIds[self];
						buffState.dst = buffState.src;
						buffState.dstUniqueId = buffState.srcUniqueId;
						buffState.values[ActionState::VStatBuff::health] = 3;
						state.stack.Add() = buffState;
						return true;
					}
				}
				return false;
			};
		arr[ARTIFACT_IDS::MANAMUNE].name = "manamune";
		arr[ARTIFACT_IDS::MANAMUNE].ruleText = "[cast] gain 1 temporary attack.";
		arr[ARTIFACT_IDS::MANAMUNE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::tempAttack] = 1;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::THORN_WHIP].name = "thorn whip";
		arr[ARTIFACT_IDS::THORN_WHIP].ruleText = "[attack] deal 1 damage to all enemies.";
		arr[ARTIFACT_IDS::THORN_WHIP].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if(actionState.src == self && actionState.srcUniqueId == boardState.uniqueIds[self])
					{
						ActionState damageState{};
						damageState.trigger = ActionState::Trigger::onDamage;
						damageState.source = ActionState::Source::board;
						damageState.src = self;
						damageState.srcUniqueId = boardState.uniqueIds[self];
						damageState.values[ActionState::VDamage::damage] = 1;
						TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
						return true;
					}
				}
				return false;
			};
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].name = "helmet of the host";
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].ruleText = "[attack] give all allies 1 temporary attack.";
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if (actionState.src != self || actionState.srcUniqueId != boardState.uniqueIds[self])
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempAttack] = 1;
					TargetOfType(info, state, buffState, self, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_SPEAR].name = "rusty spear";
		arr[ARTIFACT_IDS::RUSTY_SPEAR].ruleText = "[kill] gain 3 mana.";
		arr[ARTIFACT_IDS::RUSTY_SPEAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;
					if (actionState.source != ActionState::Source::board)
						return false;
					if (actionState.src != self || actionState.srcUniqueId != boardState.uniqueIds[self])
						return false;

					state.mana += 3;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_COLLAR].name = "rusty collar";
		arr[ARTIFACT_IDS::RUSTY_COLLAR].ruleText = "[death] draw 5 cards.";
		arr[ARTIFACT_IDS::RUSTY_COLLAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;
					if (actionState.dst != self || actionState.dstUniqueId != boardState.uniqueIds[self])
						return false;

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::board;
					drawState.src = self;
					drawState.srcUniqueId = boardState.uniqueIds[self];
					for (uint32_t i = 0; i < 5; ++i)
						state.stack.Add() = drawState;
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<uint32_t> CardGame::GetBossCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<uint32_t>(arena, BOSS_IDS::LENGTH + 3);
		uint32_t i = 0;
		for (auto& card : arr)
			card = i++;
		return arr;
	}

	jv::Array<RoomCard> CardGame::GetRoomCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<RoomCard>(arena, ROOM_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		auto& fieldOfVengeance = arr[ROOM_IDS::FIELD_OF_VENGEANCE];
		fieldOfVengeance.name = "field of vengeance";
		fieldOfVengeance.ruleText = "[monster is dealt non combat damage] attack a random enemy.";
		fieldOfVengeance.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onDamage)
			{
				if (actionState.source == ActionState::Source::board)
					return false;

				const auto& boardState = state.boardState;
				if (boardState.enemyCount == 0 || boardState.allyCount == 0)
					return false;

				ActionState attackState = actionState;
				attackState.trigger = ActionState::Trigger::onAttack;
				attackState.source = ActionState::Source::board;
				attackState.src = actionState.dst;
				attackState.dst = rand() % (actionState.dst < BOARD_CAPACITY_PER_SIDE ? boardState.enemyCount : boardState.allyCount);
				attackState.dst += (actionState.dst < BOARD_CAPACITY_PER_SIDE) * BOARD_CAPACITY_PER_SIDE;
				attackState.srcUniqueId = boardState.uniqueIds[attackState.src];
				attackState.dstUniqueId = boardState.uniqueIds[attackState.dst];
				state.stack.Add() = attackState;
				return true;
			}
			return false;
		};
		auto& forsakenBattlefield = arr[ROOM_IDS::FORSAKEN_BATTLEFIELD];
		forsakenBattlefield.name = "forsaken battlefield";
		forsakenBattlefield.ruleText = "[any non token death] fill the opposite board with vultures.";
		forsakenBattlefield.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (actionState.dstUniqueId != state.boardState.uniqueIds[actionState.dst])
						return false;
					const auto& monster = info.monsters[state.boardState.ids[actionState.dst]];
					if (monster.tags & TAG_TOKEN)
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::VULTURE;
					summonState.values[ActionState::VSummon::isAlly] = actionState.dst >= BOARD_CAPACITY_PER_SIDE;
					for (uint32_t i = 0; i < BOARD_CAPACITY_PER_SIDE; ++i)
						state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& blessedHalls = arr[ROOM_IDS::BLESSED_HALLS];
		blessedHalls.name = "blessed halls";
		blessedHalls.ruleText = "[start of turn] all enemy monsters gain +0/1 until end of turn.";
		blessedHalls.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 1;

					TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& khaalsDomain = arr[ROOM_IDS::KHAALS_DOMAIN];
		khaalsDomain.name = "khaals domain";
		khaalsDomain.ruleText = "[end of turn] deal 1 damage to all monsters.";
		khaalsDomain.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;

					TargetOfType(info, state, damageState, 0, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		auto& arenaOfTheDamned = arr[ROOM_IDS::ARENA_OF_THE_DAMNED];
		arenaOfTheDamned.name = "arena of the damned";
		arenaOfTheDamned.ruleText = "[end of turn] destroy all monsters with the lowest health.";
		arenaOfTheDamned.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;
					if (boardState.allyCount == 0 && boardState.enemyCount == 0)
						return false;

					uint32_t lowestHealth = -1;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto health = boardState.combatStats[i].health;
						if (health < lowestHealth)
							lowestHealth = health;
					}
					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						const auto health = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i].health;
						if (health < lowestHealth)
							lowestHealth = health;
					}

					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDeath;
					killState.source = ActionState::Source::other;

					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto health = boardState.combatStats[i].health;
						if (health == lowestHealth)
						{
							killState.dst = i;
							killState.dstUniqueId = boardState.uniqueIds[i];
							state.stack.Add() = killState;
						}
					}

					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						const auto health = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i].health;
						if (health == lowestHealth)
						{
							killState.dst = BOARD_CAPACITY_PER_SIDE + i;
							killState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
							state.stack.Add() = killState;
						}
					}

					return true;
				}
				return false;
			};
		auto& tranquilWaters = arr[ROOM_IDS::TRANQUIL_WATERS];
		tranquilWaters.name = "tranquil waters";
		tranquilWaters.ruleText = "[start of turn] set the attack of all monsters to 1.";
		tranquilWaters.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatSet;
					setState.source = ActionState::Source::other;
					setState.values[ActionState::VStatSet::attack] = 1;

					TargetOfType(info, state, setState, 0, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		auto& plainMeadow = arr[ROOM_IDS::PLAIN_MEADOWS];
		plainMeadow.name = "plain meadow";

		auto& prisonOfEternity = arr[ROOM_IDS::PRISON_OF_ETERNITY];
		prisonOfEternity.name = "prison of eternity";
		prisonOfEternity.ruleText = "on turn 7, kill all monsters.";
		prisonOfEternity.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.turn != 7)
						return false;

					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDeath;
					killState.source = ActionState::Source::other;

					TargetOfType(info, state, killState, 0, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<SpellCard> CardGame::GetSpellCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<SpellCard>(arena, SPELL_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		auto& arcaneIntellect = arr[SPELL_IDS::ARCANE_INTELLECT];
		arcaneIntellect.name = "arcane intellect";
		arcaneIntellect.ruleText = "draw 2.";
		arcaneIntellect.cost = 3;
		arcaneIntellect.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
			{
				ActionState drawState{};
				drawState.trigger = ActionState::Trigger::onDraw;
				drawState.source = ActionState::Source::other;
				state.stack.Add() = drawState;
				state.stack.Add() = drawState;
				return true;
			}
			return false;
		};
		auto& dreadSummon = arr[SPELL_IDS::DREAD_SUMMON];
		dreadSummon.name = "dread summon";
		dreadSummon.ruleText = "destroy all tokens and summon a x/x demon where x is the amount of tokens destroyed.";
		dreadSummon.cost = 5;
		dreadSummon.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDraw;
					killState.source = ActionState::Source::other;
					TargetOfType(info, state, killState, -1, TAG_TOKEN, TypeTarget::all);

					const auto& boardState = state.boardState;

					uint32_t c = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						if ((info.monsters[boardState.ids[i]].tags & TAG_TOKEN) == 0)
							continue;
						++c;
					}
					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						if ((info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]].tags & TAG_TOKEN) == 0)
							continue;
						++c;
					}

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::DEMON;
					summonState.values[ActionState::VSummon::attack] = c;
					summonState.values[ActionState::VSummon::health] = c;
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& tranquilize = arr[SPELL_IDS::TRANQUILIZE];
		tranquilize.name = "tranquilize";
		tranquilize.ruleText = "set a monsters attack to 1.";
		tranquilize.cost = 6;
		tranquilize.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatSet;
					setState.source = ActionState::Source::other;
					setState.dst = actionState.dst;
					setState.dstUniqueId = actionState.dstUniqueId;
					setState.values[ActionState::VStatSet::attack] = 1;
					state.stack.Add() = setState;
					return true;
				}
				return false;
			};
		auto& goblinAmbush = arr[SPELL_IDS::GOBLIN_AMBUSH];
		goblinAmbush.name = "goblin ambush";
		goblinAmbush.ruleText = "summon two goblins.";
		goblinAmbush.cost = 3;
		goblinAmbush.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onStatSet;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					state.stack.Add() = summonState;
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& windfall = arr[SPELL_IDS::WINDFALL];
		windfall.name = "windfall";
		windfall.ruleText = "discard your hand. draw that many cards.";
		windfall.cost = 3;
		windfall.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const uint32_t count = state.hand.count;
					state.hand.Clear();

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					for (uint32_t i = 0; i < count; ++i)
						state.stack.Add() = drawState;
					return true;
				}
				return false;
			};
		auto& rally = arr[SPELL_IDS::WINDFALL];
		rally.name = "rally";
		rally.ruleText = "give all allies 2 temporary attack.";
		rally.cost = 5;
		rally.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& holdTheLine = arr[SPELL_IDS::HOLD_THE_LINE];
		holdTheLine.name = "hold the line";
		holdTheLine.ruleText = "give all allies 3 temporary health.";
		holdTheLine.cost = 6;
		holdTheLine.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 3;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& rampantGrowth = arr[SPELL_IDS::RAMPANT_GROWTH];
		rampantGrowth.name = "rampant growth";
		rampantGrowth.ruleText = "gain 1 maximum mana.";
		rampantGrowth.cost = 2;
		rampantGrowth.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					++state.maxMana;
					return true;
				}
				return false;
			};
		auto& enlightenment = arr[SPELL_IDS::RAMPANT_GROWTH];
		enlightenment.name = "enlightenment";
		enlightenment.ruleText = "draw until your hand is full.";
		enlightenment.cost = 6;
		enlightenment.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					for (uint32_t i = 0; i < HAND_MAX_SIZE - state.hand.count; ++i)
						state.stack.Add() = drawState;
					return true;
				}
				return false;
			};
		auto& druidicRitual = arr[SPELL_IDS::DRUIDIC_RITUAL];
		druidicRitual.name = "druidic ritual";
		druidicRitual.ruleText = "summon two elfs.";
		druidicRitual.cost = 4;
		druidicRitual.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::ELF;
					state.stack.Add() = summonState;
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& ascension = arr[SPELL_IDS::ASCENSION];
		ascension.name = "ascension";
		ascension.ruleText = "give a token +4/4.";
		ascension.cost = 5;
		ascension.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					buffState.values[ActionState::VStatBuff::attack] = 4;
					buffState.values[ActionState::VStatBuff::health] = 4;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		auto& stampede = arr[SPELL_IDS::STAMPEDE];
		stampede.name = "stampede";
		stampede.ruleText = "give all allies 3 temporary attack.";
		stampede.cost = 7;
		stampede.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 3;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& unwind = arr[SPELL_IDS::UNWIND];
		unwind.name = "unwind";
		unwind.ruleText = "untap target monster.";
		unwind.cost = 1;
		unwind.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if (actionState.dstUniqueId != state.boardState.uniqueIds[actionState.dst])
						return false;
					if(actionState.dst >= BOARD_CAPACITY_PER_SIDE)
						return true;
					state.tapped[actionState.dst] = false;
					return true;
				}
				return false;
			};
		auto& enrage = arr[SPELL_IDS::ENRAGE];
		enrage.name = "enrage";
		enrage.ruleText = "gain 3 temporary attack.";
		enrage.cost = 1;
		enrage.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 3;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		auto& protect = arr[SPELL_IDS::PROTECT];
		protect.name = "protect";
		protect.ruleText = "gain 2 temporary health.";
		protect.cost = 1;
		protect.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		auto& infuriate = arr[SPELL_IDS::INFURIATE];
		infuriate.name = "infuriate";
		infuriate.ruleText = "gain 5 temporary attack.";
		infuriate.cost = 2;
		infuriate.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 5;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		auto& goblinChant = arr[SPELL_IDS::GOBLIN_CHANT];
		goblinChant.name = "goblin chant";
		goblinChant.ruleText = "gain 1 mana for every friendly goblin.";
		goblinChant.cost = 1;
		goblinChant.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					uint32_t m = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto& monster = info.monsters[boardState.ids[i]];
						m += monster.tags & TAG_GOBLIN;
					}
					state.mana += m;
					return true;
				}
				return false;
			};
		auto& treasureHunt = arr[SPELL_IDS::TREASURE_HUNT];
		treasureHunt.name = "treasure hunt";
		treasureHunt.ruleText = "summon three treasures for the opponent.";
		treasureHunt.cost = 2;
		treasureHunt.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 0;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::TREASURE;
					for (uint32_t i = 0; i < 3; ++i)
						state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& rewind = arr[SPELL_IDS::REWIND];
		rewind.name = "rewind";
		rewind.ruleText = "untap all allies.";
		rewind.cost = 7;
		rewind.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					for (auto& tapped : state.tapped)
						tapped = false;
					return true;
				}
				return false;
			};
		auto& manaSurge = arr[SPELL_IDS::MANA_SURGE];
		manaSurge.name = "mana surge";
		manaSurge.ruleText = "gain 5 mana.";
		manaSurge.cost = 3;
		manaSurge.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					state.mana += 5;
					return true;
				}
				return false;
			};
		auto& darkRitual = arr[SPELL_IDS::DARK_RITUAL];
		darkRitual.name = "dark ritual";
		darkRitual.ruleText = "gain 2 mana.";
		darkRitual.cost = 1;
		darkRitual.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					state.mana += 2;
					return true;
				}
				return false;
			};
		auto& flameBolt = arr[SPELL_IDS::FLAME_BOLT];
		flameBolt.name = "flame bolt";
		flameBolt.ruleText = "deal 7 damage.";
		flameBolt.cost = 5;
		flameBolt.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 7;
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		auto& pyroblast = arr[SPELL_IDS::PYROBlAST];
		pyroblast.name = "pyroblast";
		pyroblast.ruleText = "deal 10 damage.";
		pyroblast.cost = 7;
		pyroblast.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 10;
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		auto& unstableCopy = arr[SPELL_IDS::UNSTABLE_COPY];
		unstableCopy.name = "unstable copy";
		unstableCopy.ruleText = "summon a copy of target monster with 1 health remaining.";
		unstableCopy.cost = 4;
		unstableCopy.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (boardState.uniqueIds[actionState.dst] != actionState.dstUniqueId)
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::health] = 1;
					summonState.values[ActionState::VSummon::id] = boardState.ids[actionState.dst];
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& perfectCopy = arr[SPELL_IDS::PERFECT_COPY];
		perfectCopy.name = "perfect copy";
		perfectCopy.ruleText = "summon a copy of target monster.";
		perfectCopy.cost = 8;
		perfectCopy.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (boardState.uniqueIds[actionState.dst] != actionState.dstUniqueId)
						return false;

					const auto& stats = boardState.combatStats[actionState.dst];

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::health] = stats.health;
					summonState.values[ActionState::VSummon::attack] = stats.attack;
					summonState.values[ActionState::VSummon::id] = boardState.ids[actionState.dst];
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		auto& painForGain = arr[SPELL_IDS::PAIN_FOR_GAIN];
		painForGain.name = "pain for gain";
		painForGain.ruleText = "deal 3 damage. if the target was an ally, draw 3 cards.";
		painForGain.cost = 3;
		painForGain.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 3;
					state.stack.Add() = damageState;

					if(actionState.dst < BOARD_CAPACITY_PER_SIDE)
					{
						ActionState drawState{};
						drawState.trigger = ActionState::Trigger::onDraw;
						drawState.source = ActionState::Source::other;
						for (uint32_t i = 0; i < 3; ++i)
							state.stack.Add() = drawState;
					}
					
					return true;
				}
				return false;
			};
		auto& betrayal = arr[SPELL_IDS::BETRAYAL];
		betrayal.name = "betrayal";
		betrayal.ruleText = "kill target allied monster. if it was not a token, gain 5 mana.";
		betrayal.cost = 1;
		betrayal.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;

					if (boardState.uniqueIds[actionState.dst] != actionState.dstUniqueId)
						return false;

					if (actionState.dst < BOARD_CAPACITY_PER_SIDE)
					{
						ActionState deathState{};
						deathState.trigger = ActionState::Trigger::onDeath;
						deathState.source = ActionState::Source::other;
						deathState.dst = actionState.dst;
						deathState.dstUniqueId = actionState.dstUniqueId;
						state.stack.Add() = deathState;

						const auto& monster = info.monsters[boardState.ids[actionState.dst]];
						if((monster.tags & TAG_TOKEN) == 0)
							state.mana += 5;
						return true;
					}
				}
				return false;
			};
		auto& incantationOfDoom = arr[SPELL_IDS::INCANTATION_OF_DOOM];
		incantationOfDoom.name = "incantation of doom";
		incantationOfDoom.ruleText = "deal 13 damage to all enemies.";
		incantationOfDoom.cost = 13;
		incantationOfDoom.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 13;
					TargetOfType(info, state, damageState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<CurseCard> CardGame::GetCurseCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<CurseCard>(arena, CURSE_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		arr[CURSE_IDS::FADING].name = "curse of fading";
		arr[CURSE_IDS::FADING].ruleText = "[start of Turn] take one damage.";
		arr[CURSE_IDS::FADING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = self;
					damageState.dstUniqueId = state.boardState.uniqueIds[self];
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::WEAKNESS].name = "curse of weakness";
		arr[CURSE_IDS::WEAKNESS].ruleText = "[start of Turn] set your attack to 1.";
		arr[CURSE_IDS::WEAKNESS].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatSet;
					setState.source = ActionState::Source::other;
					setState.dst = self;
					setState.dstUniqueId = state.boardState.uniqueIds[self];
					setState.values[ActionState::VStatSet::attack] = 1;
					state.stack.Add() = setState;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::COWARDICE].name = "curse of cowardice";
		arr[CURSE_IDS::COWARDICE].ruleText = "[any attack] tap.";
		arr[CURSE_IDS::COWARDICE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					state.tapped[self] = true;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::DUM_DUM].name = "curse of dum dum";
		arr[CURSE_IDS::DUM_DUM].ruleText = "[start of turn] set your maximum mana to 5 if it is higher than that.";
		arr[CURSE_IDS::DUM_DUM].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.maxMana <= 5)
						return false;
					state.maxMana = 5;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::HATE].name = "curse of hate";
		arr[CURSE_IDS::HATE].ruleText = "[start of Turn] gain 2 attack, take 2 damage.";
		arr[CURSE_IDS::HATE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState dmgState{};
					dmgState.trigger = ActionState::Trigger::onDamage;
					dmgState.source = ActionState::Source::other;
					dmgState.dst = self;
					dmgState.dstUniqueId = state.boardState.uniqueIds[self];
					dmgState.values[ActionState::VDamage::damage] = 2;
					state.stack.Add() = dmgState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 2;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::HAUNTING].name = "curse of haunting";
		arr[CURSE_IDS::HAUNTING].ruleText = "[attack] summon an amount of goblins equal to the damage dealt for the opponent.";
		arr[CURSE_IDS::HAUNTING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.src != self || actionState.srcUniqueId != state.boardState.uniqueIds[self])
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 0;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;

					auto dmg = actionState.values[ActionState::VDamage::damage];
					dmg = jv::Min(dmg, BOARD_CAPACITY_PER_SIDE);
					for (uint32_t i = 0; i < dmg; ++i)
						state.stack.Add() = summonState;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::TIME].name = "curse of time";
		arr[CURSE_IDS::TIME].ruleText = "[start of turn 7] dies.";
		arr[CURSE_IDS::TIME].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.turn != 7)
						return false;

					ActionState deathState{};
					deathState.trigger = ActionState::Trigger::onDeath;
					deathState.source = ActionState::Source::other;
					deathState.dst = self;
					deathState.dstUniqueId = state.boardState.uniqueIds[self];
					state.stack.Add() = deathState;
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<EventCard> CardGame::GetEventCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<EventCard>(arena, EVENT_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		auto& aetherSurge = arr[EVENT_IDS::AETHER_SURGE];
		aetherSurge.name = "aether surge";
		aetherSurge.ruleText = "[card played] all enemies gain +1/1.";
		aetherSurge.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCast)
			{
				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::attack] = 1;
				buffState.values[ActionState::VStatBuff::health] = 1;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			}
			return false;
		};
		auto& doppleGangsters = arr[EVENT_IDS::DOPPLEGANGSTERS];
		doppleGangsters.name = "dopplegangsters";
		doppleGangsters.ruleText = "[summon ally] summon a copy for your opponent.";
		doppleGangsters.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onSummon)
				{
					if (!actionState.values[ActionState::VSummon::isAlly])
						return false;
					ActionState summonState = actionState;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 0;
					state.stack.Add() = summonState;
					return true;
				}
				return false;
			};

		auto& gatheringStorm = arr[EVENT_IDS::GATHERING_STORM];
		gatheringStorm.name = "gathering storm";
		gatheringStorm.ruleText = "[end of turn] give all enemies +1/1.";
		gatheringStorm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onEndOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::attack] = 1;
				buffState.values[ActionState::VStatBuff::health] = 1;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			};

		auto& noYou = arr[EVENT_IDS::NO_YOU];
		noYou.name = "no you";
		noYou.ruleText = "[ally attack] attack back.";
		noYou.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onAttack)
					return false;
				if (actionState.src >= BOARD_CAPACITY_PER_SIDE)
					return false;

				auto attackState = actionState;
				attackState.src = actionState.dst;
				attackState.srcUniqueId = actionState.dstUniqueId;
				attackState.dst = actionState.src;
				attackState.dstUniqueId = actionState.srcUniqueId;
				state.stack.Add() = attackState;
				return true;
			};
		auto& goblinPlague = arr[EVENT_IDS::GOBLIN_PLAGUE];
		goblinPlague.name = "goblin plague";
		goblinPlague.ruleText = "[end of turn] summon 2 goblins.";
		goblinPlague.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onEndOfTurn)
					return false;

				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.source = ActionState::Source::other;
				summonState.values[ActionState::VSummon::isAlly] = 0;
				summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
				state.stack.Add() = summonState;
				state.stack.Add() = summonState;
				return true;
			};
		auto& whirlwind = arr[EVENT_IDS::WHIRLWIND];
		whirlwind.name = "whirlwind";
		whirlwind.ruleText = "[end of turn] deal 1 damage to all monsters.";
		whirlwind.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onEndOfTurn)
					return false;

				ActionState damageState{};
				damageState.trigger = ActionState::Trigger::onDamage;
				damageState.source = ActionState::Source::other;
				damageState.values[ActionState::VDamage::damage] = 1;
				TargetOfType(info, state, damageState, 0, -1, TypeTarget::all);
				return true;
			};

		auto& healingWord = arr[EVENT_IDS::HEALING_WORD];
		healingWord.name = "healing word";
		healingWord.ruleText = "[start of turn] all enemies gain 3 health until end of turn.";
		healingWord.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::tempHealth] = 3;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			};

		auto& briefRespite = arr[EVENT_IDS::BRIEF_RESPISE];
		briefRespite.name = "brief respite";

		auto& chaseTheDragon = arr[EVENT_IDS::CHASE_THE_DRAGON];
		chaseTheDragon.name = "chase the dragon";
		chaseTheDragon.ruleText = "[ally attack] change all other targets to the attacker.";
		chaseTheDragon.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger != ActionState::Trigger::onAttack)
				return false;
			if (actionState.src >= BOARD_CAPACITY_PER_SIDE)
				return false;

			for (auto& target : state.targets)
				target = actionState.src;
			return true;
		};
		return arr;
	}

	void CardGame::UpdateInput()
	{
		const auto mousePos = jv::ge::GetMousePosition();
		
		inputState.fullScreenMousePos = mousePos;
		inputState.mousePos = PixelPerfectRenderTask::ToPixelPosition(
			jv::ge::GetResolution(), SIMULATED_RESOLUTION, mousePos);
		inputState.scroll = scrollCallback;

		inputState.lMouse.change = false;
		inputState.rMouse.change = false;
		inputState.enter.change = false;
		
		for (const auto& callback : mouseCallbacks)
		{
			SetInputState(inputState.lMouse, GLFW_MOUSE_BUTTON_LEFT, callback);
			SetInputState(inputState.rMouse, GLFW_MOUSE_BUTTON_RIGHT, callback);
		}
		for (const auto& callback : keyCallbacks)
			SetInputState(inputState.enter, GLFW_KEY_ENTER, callback);
		
		// Reset callbacks.
		keyCallbacks = {};
		mouseCallbacks = {};
		scrollCallback = 0;
	}

	void CardGame::SetInputState(InputState::State& state, const uint32_t target, const KeyCallback callback)
	{
		if (callback.key == target)
		{
			if (callback.action == GLFW_PRESS)
			{
				state.pressed = true;
				state.change = true;
			}
			else if (callback.action == GLFW_RELEASE)
			{
				state.pressed = false;
				state.change = true;
			}
		}
	}

	void CardGame::OnKeyCallback(const size_t key, const size_t action)
	{
		KeyCallback callback{};
		callback.key = key;
		callback.action = action;
		const auto mem = cardGame.engine.GetMemory();
		Add(mem.frameArena, cardGame.keyCallbacks) = callback;
	}

	void CardGame::OnMouseCallback(const size_t key, const size_t action)
	{
		KeyCallback callback{};
		callback.key = key;
		callback.action = action;
		const auto mem = cardGame.engine.GetMemory();
		Add(mem.frameArena, cardGame.mouseCallbacks) = callback;
	}

	void CardGame::OnScrollCallback(const glm::vec<2, double> offset)
	{
		cardGame.scrollCallback += static_cast<float>(offset.y);
	}

	void Start()
	{
		assert(!cardGameRunning);
		cardGame.Create(&cardGame);
		cardGameRunning = true;
	}

	bool Update()
	{
		assert(cardGameRunning);
		return cardGame.Update();
	}

	void Stop()
	{
		assert(cardGameRunning);
		cardGame.Destroy(cardGame);
		cardGame = {};
	}

	bool TryLoadSaveData(PlayerState& playerState)
	{
		assert(cardGameRunning);
		std::ifstream inFile;
		inFile.open(SAVE_DATA_PATH);
		if (!inFile.is_open())
		{
			inFile.close();
			return false;
		}

		for (auto& monsterId : playerState.monsterIds)
			inFile >> monsterId;
		for (auto& artifact : playerState.artifacts)
			inFile >> artifact;
		for (auto &artifactSlotCount : playerState.artifactSlotCounts)
			inFile >> artifactSlotCount;
		inFile >> playerState.partySize;
		inFile >> playerState.ironManMode;
		inFile.close();
		return true;
	}

	void SaveData(PlayerState& playerState)
	{
		assert(cardGameRunning);
		std::ofstream outFile;
		outFile.open(SAVE_DATA_PATH);

		for (const auto& monsterId : playerState.monsterIds)
			outFile << monsterId << std::endl;
		for (const auto& artifact : playerState.artifacts)
			outFile << artifact << std::endl;
		for (const auto& artifactSlotCount : playerState.artifactSlotCounts)
			outFile << artifactSlotCount << std::endl;
		outFile << playerState.partySize << std::endl;
		outFile << playerState.ironManMode << std::endl;
		outFile.close();
	}

	void ClearSaveData()
	{
		assert(cardGameRunning);
		remove(SAVE_DATA_PATH);
	}
}
