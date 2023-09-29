#include "pch_game.h"
#include "CardGame.h"

#include <chrono>
#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>
#include "Cards/ArtifactCard.h"
#include "Cards/MagicCard.h"
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
		jv::Array<MagicCard> magic;
		jv::Array<FlawCard> flaws;
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
		[[nodiscard]] static jv::Array<MagicCard> GetMagicCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<FlawCard> GetFlawCards(jv::Arena& arena);
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
				magic,
				flaws,
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
			magic,
			flaws,
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
			outCardGame->magic = cardGame.GetMagicCards(outCardGame->arena);
			outCardGame->flaws = cardGame.GetFlawCards(outCardGame->arena);
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

	jv::Array<MonsterCard> CardGame::GetMonsterCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MonsterCard>(arena, MONSTER_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
		{
			card.name = "monster";
			card.animIndex = c++;
		}
		arr[MONSTER_IDS::MANA_SLIME].name = "mama slime";
		arr[MONSTER_IDS::MANA_SLIME].attack = 1;
		arr[MONSTER_IDS::MANA_SLIME].health = 6;
		arr[MONSTER_IDS::MANA_SLIME].ruleText = "[end of turn] summon a slime.";
		arr[MONSTER_IDS::MANA_SLIME].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger != ActionState::Trigger::onEndOfTurn)
				return false;
			
			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = self < BOARD_CAPACITY_PER_SIDE;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = 1;
			state.stack.Add() = summonState;
			return true;
		};
		arr[MONSTER_IDS::MANA_SLIME].animIndex = 2;
		arr[MONSTER_IDS::MANA_SLIME].normalAnimIndex = 2 + 30;
		arr[MONSTER_IDS::MANA_SLIME].tags = static_cast<uint32_t>(MonsterCard::Tag::slime);

		arr[MONSTER_IDS::SLIME].name = "slime";
		arr[MONSTER_IDS::SLIME].attack = 1;
		arr[MONSTER_IDS::SLIME].health = 1;
		arr[MONSTER_IDS::SLIME].unique = true;
		arr[MONSTER_IDS::SLIME].tags = static_cast<uint32_t>(MonsterCard::Tag::slime);
		arr[MONSTER_IDS::SLIME].tags += static_cast<uint32_t>(MonsterCard::Tag::token);

		arr[MONSTER_IDS::HUNTED_DRAKE].name = "hunted drake";
		arr[MONSTER_IDS::HUNTED_DRAKE].attack = 4;
		arr[MONSTER_IDS::HUNTED_DRAKE].health = 12;
		arr[MONSTER_IDS::HUNTED_DRAKE].ruleText = "[attack] summon two soldiers for the opponent.";
		arr[MONSTER_IDS::HUNTED_DRAKE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger != ActionState::Trigger::onAttack)
				return false;
			if (actionState.src != self)
				return false;

			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = self >= BOARD_CAPACITY_PER_SIDE;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = 3;
			state.stack.Add() = summonState;
			state.stack.Add() = summonState;

			return true;
		};
		arr[MONSTER_IDS::HUNTED_DRAKE].tags = static_cast<uint32_t>(MonsterCard::Tag::dragon);

		arr[MONSTER_IDS::SOLDIER].name = "soldier";
		arr[MONSTER_IDS::SOLDIER].attack = 1;
		arr[MONSTER_IDS::SOLDIER].health = 1;
		arr[MONSTER_IDS::SOLDIER].unique = true;
		arr[MONSTER_IDS::SOLDIER].tags = static_cast<uint32_t>(MonsterCard::Tag::human);
		arr[MONSTER_IDS::SOLDIER].tags += static_cast<uint32_t>(MonsterCard::Tag::token);

		arr[MONSTER_IDS::ARBOR_ELF].name = "arbor elf";
		arr[MONSTER_IDS::ARBOR_ELF].attack = 1;
		arr[MONSTER_IDS::ARBOR_ELF].health = 9;
		arr[MONSTER_IDS::ARBOR_ELF].ruleText = "[start of turn] gain one mana.";
		arr[MONSTER_IDS::ARBOR_ELF].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
				return false;

			++state.maxMana;
			return true;
		};
		arr[MONSTER_IDS::ARBOR_ELF].tags = static_cast<uint32_t>(MonsterCard::Tag::elf);

		arr[MONSTER_IDS::FINAL_BOSS].unique = true;
		arr[MONSTER_IDS::FINAL_BOSS].name = "da true final boss";
		arr[MONSTER_IDS::FINAL_BOSS].health = 44;
		arr[MONSTER_IDS::FINAL_BOSS].attack = 104;

		arr[MONSTER_IDS::GOBLIN].name = "goblin";
		arr[MONSTER_IDS::GOBLIN].attack = 1;
		arr[MONSTER_IDS::GOBLIN].health = 1;
		arr[MONSTER_IDS::GOBLIN].unique = true;
		arr[MONSTER_IDS::GOBLIN].tags = static_cast<uint32_t>(MonsterCard::Tag::goblin);
		arr[MONSTER_IDS::GOBLIN].tags += static_cast<uint32_t>(MonsterCard::Tag::token);

		arr[MONSTER_IDS::DAISY].name = "daisy";
		arr[MONSTER_IDS::DAISY].attack = 4;
		arr[MONSTER_IDS::DAISY].health = 4;
		arr[MONSTER_IDS::DAISY].unique = true;
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, ARTIFACT_IDS::LENGTH);

		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		arr[ARTIFACT_IDS::CROWN_OF_MALICE].name = "crown of malice";
		arr[ARTIFACT_IDS::CROWN_OF_MALICE].ruleText = "[on any death] gain +1/+1.";
		arr[ARTIFACT_IDS::CROWN_OF_MALICE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onDeath)
			{
				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onBuff;
				buffState.source = ActionState::Source::board;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::attack)] = 1;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::health)] = 1;
				buffState.dst = self;
				buffState.dstUniqueId = state.boardState.uniqueIds[self];
				return true;
			}
			return false;
		};
		arr[ARTIFACT_IDS::BOOK_OF_KNOWLEDGE].name = "book of knowledge";
		arr[ARTIFACT_IDS::BOOK_OF_KNOWLEDGE].ruleText = "[on card played] gain +1/+1.";
		arr[ARTIFACT_IDS::BOOK_OF_KNOWLEDGE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed)
			{
				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onBuff;
				buffState.source = ActionState::Source::board;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::attack)] = 1;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::health)] = 1;
				buffState.dst = self;
				buffState.dstUniqueId = state.boardState.uniqueIds[self];
				return true;
			}
			return false;
		};
		arr[ARTIFACT_IDS::ARMOR_OF_THORNS].name = "armor of thorns";
		arr[ARTIFACT_IDS::ARMOR_OF_THORNS].ruleText = "[on attacked] deal 2 damage to the attacker.";
		arr[ARTIFACT_IDS::ARMOR_OF_THORNS].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onAttack && actionState.dst == self)
			{
				ActionState damageState{};
				damageState.trigger = ActionState::Trigger::onDamage;
				damageState.source = ActionState::Source::board;
				damageState.values[static_cast<uint32_t>(ActionState::VDamage::damage)] = 2;
				damageState.dst = actionState.src;
				damageState.dstUniqueId = actionState.srcUniqueId;
				damageState.src = actionState.dst;
				damageState.srcUniqueId = actionState.dstUniqueId;
				state.stack.Add() = damageState;
				return true;
			}
			return false;
		};
		return arr;
	}

	jv::Array<uint32_t> CardGame::GetBossCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<uint32_t>(arena, BOSS_IDS::LENGTH);
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

		arr[ROOM_IDS::FIELD_OF_CARNAGE].name = "field of carnage";
		arr[ROOM_IDS::FIELD_OF_CARNAGE].ruleText = "[monster is dealt damage] it attacks a random enemy monster.";
		arr[ROOM_IDS::FIELD_OF_CARNAGE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onDamage)
			{
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
		return arr;
	}

	jv::Array<MagicCard> CardGame::GetMagicCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MagicCard>(arena, MAGIC_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		arr[MAGIC_IDS::LIGHTNING_BOLT].name = "lightning bolt";
		arr[MAGIC_IDS::LIGHTNING_BOLT].ruleText = "deal 2 damage.";
		arr[MAGIC_IDS::LIGHTNING_BOLT].animIndex = 24;
		arr[MAGIC_IDS::LIGHTNING_BOLT].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				ActionState damageState{};
				damageState.trigger = ActionState::Trigger::onDamage;
				damageState.source = ActionState::Source::other;
				damageState.values[static_cast<uint32_t>(ActionState::VDamage::damage)] = 2;
				damageState.dst = actionState.dst;
				damageState.dstUniqueId = actionState.dstUniqueId;
				state.stack.Add() = damageState;
				return true;
			}
			return false;
		};
		arr[MAGIC_IDS::GATHER_THE_WEAK].name = "gather the weak";
		arr[MAGIC_IDS::GATHER_THE_WEAK].ruleText = "summon two goblins";
		arr[MAGIC_IDS::GATHER_THE_WEAK].type = MagicCard::Type::all;
		arr[MAGIC_IDS::GATHER_THE_WEAK].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.source = ActionState::Source::other;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = MONSTER_IDS::GOBLIN;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 1;
				state.stack.Add() = summonState;
				state.stack.Add() = summonState;
				return true;
			}
			return false;
		};

		arr[MAGIC_IDS::GOBLIN_SUPREMACY].name = "goblin supremacy";
		arr[MAGIC_IDS::GOBLIN_SUPREMACY].ruleText = "give all goblins +2/+1.";
		arr[MAGIC_IDS::GOBLIN_SUPREMACY].type = MagicCard::Type::all;
		arr[MAGIC_IDS::GOBLIN_SUPREMACY].cost = 2;
		arr[MAGIC_IDS::GOBLIN_SUPREMACY].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::attack)] = 2;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::health)] = 1;

				const auto& boardState = state.boardState;
				for (uint32_t i = 0; i < boardState.allyCount; ++i)
				{
					if ((info.monsters[boardState.ids[i]].tags & TAG_GOBLIN) == 0)
						continue;
					buffState.dst = i;
					buffState.dstUniqueId = boardState.uniqueIds[i];
					state.stack.Add() = buffState;
				}
				for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				{
					if ((info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]].tags & TAG_GOBLIN) == 0)
						continue;
					buffState.dst = BOARD_CAPACITY_PER_SIDE + i;
					buffState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
					state.stack.Add() = buffState;
				}
				
				return true;
			}
			return false;
		};
		arr[MAGIC_IDS::CULL_THE_MEEK].name = "cull the meek";
		arr[MAGIC_IDS::CULL_THE_MEEK].ruleText = "destroy all friendly tokens to give all allies +2/+2 for each monster destroyed.";
		arr[MAGIC_IDS::CULL_THE_MEEK].type = MagicCard::Type::all;
		arr[MAGIC_IDS::CULL_THE_MEEK].cost = 3;
		arr[MAGIC_IDS::CULL_THE_MEEK].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				ActionState killState{};
				killState.trigger = ActionState::Trigger::onDeath;
				killState.source = ActionState::Source::other;

				const auto& boardState = state.boardState;
				uint32_t count = 0;

				for (uint32_t i = 0; i < boardState.allyCount; ++i)
				{
					if ((info.monsters[boardState.ids[i]].tags & TAG_TOKEN) == 0)
						continue;
					killState.dst = i;
					killState.dstUniqueId = boardState.uniqueIds[i];
					state.stack.Add() = killState;
					++count;
				}

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::attack)] = 2 * count;
				buffState.values[static_cast<uint32_t>(ActionState::VBuff::health)] = 2 * count;

				for (uint32_t i = 0; i < boardState.allyCount; ++i)
				{
					buffState.dst = i;
					buffState.dstUniqueId = boardState.uniqueIds[i];
					state.stack.Add() = buffState;
				}

				return true;
			}
			return false;
		};
		arr[MAGIC_IDS::SECOND_WIND].name = "second wind";
		arr[MAGIC_IDS::SECOND_WIND].ruleText = "ready all monsters.";
		arr[MAGIC_IDS::SECOND_WIND].type = MagicCard::Type::all;
		arr[MAGIC_IDS::SECOND_WIND].cost = 5;
		arr[MAGIC_IDS::SECOND_WIND].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				for (auto& t : state.tapped)
					t = false;
				return true;
			}
			return false;
		};
		arr[MAGIC_IDS::PARIAH].name = "pariah";
		arr[MAGIC_IDS::PARIAH].ruleText = "change all enemy targets to this monster.";
		arr[MAGIC_IDS::PARIAH].cost = 3;
		arr[MAGIC_IDS::PARIAH].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCardPlayed && self == actionState.src)
			{
				for (auto& t : state.targets)
					t = actionState.dst;
				return true;
			}
			return false;
		};

		return arr;
	}

	jv::Array<FlawCard> CardGame::GetFlawCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<FlawCard>(arena, FLAW_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		for (auto& card : arr)
			card.name = "curse";
		return arr;
	}

	jv::Array<EventCard> CardGame::GetEventCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<EventCard>(arena, EVENT_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		for (auto& card : arr)
			card.name = "event";
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
