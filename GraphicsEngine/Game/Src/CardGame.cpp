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
#include "States/GameState.h"
#include "States/InputState.h"
#include <time.h>

#include "JLib/Math.h"
#include "RenderGraph/RenderGraph.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";
	constexpr const char* RESOLUTION_DATA_PATH = "Resolution.txt";

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
			float p1Lerp;
			float p2Lerp;
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

		glm::ivec2 resolution;
		bool isFullScreen;

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

		bool activePlayer;
		bool inCombat;
		float p1Lerp, p2Lerp;
		bool restart;

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

	void SaveResolution(const glm::ivec2 resolution, const bool fullScreen)
	{
		std::ofstream outFile;
		outFile.open(RESOLUTION_DATA_PATH);
		outFile << fullScreen << std::endl;
		outFile << resolution.x << std::endl;
		outFile << resolution.y << std::endl;
		outFile.close();
	}

	void LoadResolution(glm::ivec2& resolution, bool& fullScreen)
	{
		std::ifstream inFile;
		inFile.open(RESOLUTION_DATA_PATH);
		if (!inFile.is_open())
		{
			glm::ivec2 res = SIMULATED_RESOLUTION * 3;
			SaveResolution(res, false);
			inFile.close();
			
			LoadResolution(resolution, fullScreen);
			return;
		}

		inFile >> fullScreen;
		inFile >> resolution.x;
		inFile >> resolution.y;

		inFile.close();
	}

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

		inCombat = false;
		if(!inCombat)
		{
			p1Lerp -= dt;
			p2Lerp -= dt;
		}
		else
		{
			p1Lerp += dt * 2 * (2 * activePlayer - 1);
			p2Lerp += dt * 2 * (2 * !activePlayer - 1);
		}
		p1Lerp = jv::Clamp<float>(p1Lerp, 0.5, 1);
		p2Lerp = jv::Clamp<float>(p2Lerp, 0.5, 1);

		bool fullscreen = isFullScreen;
		
		const LevelUpdateInfo info
		{
			levelArena,
			engine.GetMemory().tempArena,
			engine.GetMemory().frameArena,
			levelScene,
			atlasTextures,
			gameState,
			monsters,
			artifacts,
			bosses,
			rooms,
			spells,
			curses,
			events,
			resolution,
			inputState,
			*textTasks,
			*pixelPerfectRenderTasks,
			*lightTasks,
			dt,
			textureStreamer,
			screenShakeInfo,
			pixelation,
			activePlayer,
			inCombat,
			fullscreen
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

		if (fullscreen != isFullScreen)
		{
			// Restart.
			glm::ivec2 res;
			bool fs;
			LoadResolution(res, fs);
			SaveResolution(res, fullscreen);
			restart = true;
			return false;
		}

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
			pushConstant.time = cardGame->timeSinceStarted + 10;
			pushConstant.resolution = cardGame->resolution;
			pushConstant.simResolution = SIMULATED_RESOLUTION;
			pushConstant.pixelation = cardGame->pixelation;
			pushConstant.p1Lerp = cardGame->p1Lerp;
			pushConstant.p2Lerp = cardGame->p2Lerp;

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

		glm::ivec2 res;
		bool fullScreen;
		LoadResolution(res, fullScreen);
		res.x = jv::Max(res.x, SIMULATED_RESOLUTION.x);
		res.y = jv::Max(res.y, SIMULATED_RESOLUTION.y);

		*outCardGame = {};
		{
			EngineCreateInfo engineCreateInfo{};
			engineCreateInfo.onKeyCallback = OnKeyCallback;
			engineCreateInfo.onMouseCallback = OnMouseCallback;
			engineCreateInfo.onScrollCallback = OnScrollCallback;
			engineCreateInfo.resolution = res;
			engineCreateInfo.fullScreen = fullScreen;
			outCardGame->engine = Engine::Create(engineCreateInfo);
		}
		outCardGame->restart = false;

		res = Engine::GetResolution();
		outCardGame->resolution = res;
		outCardGame->isFullScreen = fullScreen;
		
		outCardGame->arena = outCardGame->engine.CreateSubArena(1024);
		outCardGame->levelArena = outCardGame->engine.CreateSubArena(1024);
		outCardGame->scene = jv::ge::CreateScene();
		outCardGame->levelScene = jv::ge::CreateScene();

		outCardGame->activePlayer = false;
		outCardGame->inCombat = false;
		outCardGame->p1Lerp = outCardGame->p2Lerp = 0;

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
				samplerCreateInfo.addressModeU = jv::ge::SamplerCreateInfo::AddressMode::clampToBorder;
				samplerCreateInfo.addressModeV = jv::ge::SamplerCreateInfo::AddressMode::clampToBorder;
				samplerCreateInfo.addressModeW = jv::ge::SamplerCreateInfo::AddressMode::clampToBorder;
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
			pipelineCreateInfo.resolution = res;
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
			outCardGame->pixelPerfectRenderTasks->Allocate(outCardGame->arena, 256);
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
			dynamicEnableInfo.capacity = 64;

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
			outCardGame->levels = jv::CreateArray<Level*>(outCardGame->arena, 4);
			outCardGame->levels[0] = outCardGame->arena.New<MainMenuLevel>();
			outCardGame->levels[1] = outCardGame->arena.New<NewGameLevel>();
			outCardGame->levels[2] = outCardGame->arena.New<MainLevel>();
			outCardGame->levels[3] = outCardGame->arena.New<GameOverLevel>();
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
		arr[5] = "Art/button.png";
		arr[6] = "Art/stats.png";
		arr[7] = "Art/flowers.png";
		arr[8] = "Art/fallback.png";
		arr[9] = "Art/empty.png";
		return arr;
	}

	jv::Array<const char*> CardGame::GetDynamicTexturePaths(jv::Arena& arena, jv::Arena& frameArena)
	{
		constexpr uint32_t l = 45;
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

	void TargetOfType(const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self, const uint32_t tags, const TypeTarget target, bool excludeSelf = false)
	{
		ActionState cpyState = actionState;

		const auto& boardState = state.boardState;
		if(target == TypeTarget::all || (self < BOARD_CAPACITY_PER_SIDE && target == TypeTarget::allies || 
			self >= BOARD_CAPACITY_PER_SIDE && target == TypeTarget::enemies))
			for (uint32_t i = 0; i < boardState.allyCount; ++i)
			{
				if (excludeSelf && i == self)
					continue;
				if (tags != -1 && (info.monsters[boardState.ids[i]].tags & tags) == 0)
					continue;
				cpyState.dst = i;
				cpyState.dstUniqueId = boardState.uniqueIds[i];
				state.TryAddToStack(cpyState);
			}
		if (target == TypeTarget::all || (self >= BOARD_CAPACITY_PER_SIDE && target == TypeTarget::allies || 
			self < BOARD_CAPACITY_PER_SIDE && target == TypeTarget::enemies))
			for (uint32_t i = 0; i < boardState.enemyCount; ++i)
			{
				if (excludeSelf && BOARD_CAPACITY_PER_SIDE + i == self)
					continue;
				if (tags != -1 && (info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]].tags & tags) == 0)
					continue;
				cpyState.dst = BOARD_CAPACITY_PER_SIDE + i;
				cpyState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
				state.TryAddToStack(cpyState);
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
		goblin.attack = 2;
		goblin.health = 1;
		goblin.tags = TAG_TOKEN | TAG_GOBLIN;
		goblin.unique = true;
		goblin.ruleText = "[summoned] untap. [attack] die.";
		goblin.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onSummon)
				{
					const uint32_t isAlly = actionState.values[ActionState::VSummon::isAlly];
					if (isAlly && self < BOARD_CAPACITY_PER_SIDE)
						if (state.boardState.allyCount - 1 == self)
						{
							state.tapped[self] = false;
							return true;
						}
				}
				if(actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src == self)
					{
						ActionState killState{};
						killState.trigger = ActionState::Trigger::onDeath;
						killState.source = ActionState::Source::other;
						killState.dst = self;
						killState.dstUniqueId = state.boardState.uniqueIds[self];
						state.TryAddToStack(killState);
						return true;
					}
				}
				return false;
			};
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
		elf.ruleText = "[start of turn] +1 mana.";
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
		treasure.ruleText = "[death] +1 mana.";
		treasure.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (actionState.dst != self)
						return false;
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;
					++state.mana;
					return true;
				}
				return false;
			};
		auto& slime = arr[MONSTER_IDS::SLIME];
		slime.name = "slime";
		slime.attack = 0;
		slime.health = 0;
		slime.unique = true;
		slime.tags = TAG_TOKEN | TAG_SLIME;
		auto& daisy = arr[MONSTER_IDS::DAISY];
		daisy.name = "daisy";
		daisy.ruleText = "loyal until the very end.";
		daisy.attack = 3;
		daisy.health = 3;
		daisy.unique = true;
		daisy.animIndex = 2;
		daisy.normalAnimIndex = 47;
		auto& god = arr[MONSTER_IDS::GOD];
		god.name = "god";
		god.attack = 0;
		god.health = 999;
		god.unique = true;
		god.ruleText = "[attacked, cast] deal 1 damage to all enemies and gain 1 attack.";
		god.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast || actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.trigger == ActionState::Trigger::onAttack && actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = buffState.src;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatBuff::attack] = 1;
					state.TryAddToStack(buffState);

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& greatTroll = arr[MONSTER_IDS::GREAT_TROLL];
		greatTroll.name = "great troll";
		greatTroll.attack = 5;
		greatTroll.health = 75;
		greatTroll.ruleText = "[attacked, cast] +1 bonus attack.";
		greatTroll.unique = true;
		greatTroll.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast || actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.trigger == ActionState::Trigger::onAttack && actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempAttack] = 1;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& slimeQueen = arr[MONSTER_IDS::SLIME_QUEEN];
		slimeQueen.name = "slime queen";
		slimeQueen.attack = 2;
		slimeQueen.health = 35;
		slimeQueen.ruleText = "[end of turn] summon a copy.";
		slimeQueen.unique = true;
		slimeQueen.tags = TAG_SLIME;
		slimeQueen.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;
					const auto& stats = boardState.combatStats[self];
					
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::SLIME_QUEEN;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					summonState.values[ActionState::VSummon::health] = stats.health + stats.tempHealth;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& lichKing = arr[MONSTER_IDS::LICH_KING];
		lichKing.name = "lich king";
		lichKing.attack = 0;
		lichKing.health = 100;
		lichKing.ruleText = "[start of turn] +1 attack and +3 bonus health.";
		lichKing.unique = true;
		lichKing.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 1;
					buffState.values[ActionState::VStatBuff::tempHealth] = 3;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& mirrorKnight = arr[MONSTER_IDS::MIRROR_KNIGHT];
		mirrorKnight.name = "mirror knight";
		mirrorKnight.attack = 4;
		mirrorKnight.health = 100;
		mirrorKnight.ruleText = "[start of turn] summon 2/1 copies of each enemy monster.";
		mirrorKnight.unique = true;
		mirrorKnight.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					const auto& boardState = state.boardState;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						ActionState summonState{};
						summonState.trigger = ActionState::Trigger::onSummon;
						summonState.source = ActionState::Source::other;
						summonState.values[ActionState::VSummon::id] = boardState.ids[i];
						summonState.values[ActionState::VSummon::isAlly] = false;
						summonState.values[ActionState::VSummon::attack] = 2;
						summonState.values[ActionState::VSummon::health] = 1;
						state.TryAddToStack(summonState);
					}
					
					return true;
				}
				return false;
			};
		auto& bomba = arr[MONSTER_IDS::BOMBA];
		bomba.name = "bomba";
		bomba.attack = 4;
		bomba.health = 140;
		bomba.ruleText = "[cast, attacked] summon a bomb.";
		bomba.unique = true;
		bomba.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				const bool attacked = actionState.trigger == ActionState::Trigger::onAttack && actionState.dst == self;

				if (attacked || actionState.trigger == ActionState::Trigger::onCast)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 0;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::BOMB;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& bomb = arr[MONSTER_IDS::BOMB];
		bomb.name = "bomb";
		bomb.attack = 0;
		bomb.health = 2;
		bomb.ruleText = "[end of turn] die and deal 4 damage to all enemies.";
		bomb.unique = true;
		bomb.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDeath;
					killState.source = ActionState::Source::other;
					killState.dst = self;
					killState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(killState);

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 4;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& theDread = arr[MONSTER_IDS::THE_DREAD];
		theDread.name = "the dread";
		theDread.attack = 0;
		theDread.health = 666;
		theDread.ruleText = "[start of turn] gain +6 attack. [turn 6] dies.";
		theDread.unique = true;
		theDread.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if(state.turn == 7)
					{
						ActionState killState{};
						killState.trigger = ActionState::Trigger::onDeath;
						killState.source = ActionState::Source::other;
						killState.dst = self;
						killState.dstUniqueId = state.boardState.uniqueIds[self];
						state.TryAddToStack(killState);
						return true;
					}

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 6;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& archmage = arr[MONSTER_IDS::ARCHMAGE];
		archmage.name = "archmage";
		archmage.attack = 8;
		archmage.health = 200;
		archmage.ruleText = "[on enemy buff] copy it for myself. [end of turn] deal 2 damage to all enemies.";
		archmage.unique = true;
		archmage.tags = TAG_HUMAN;
		archmage.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 2;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
					return true;
				}
				if (actionState.trigger == ActionState::Trigger::onStatBuff)
				{
					if (actionState.dst == self)
						return false;
					auto cpy = actionState;
					cpy.dst = self;
					cpy.dstUniqueId = state.boardState.uniqueIds[self];
					state.stack.Add() = cpy;
					return true;
				}
				return false;
			};
		auto& shelly = arr[MONSTER_IDS::SHELLY];
		shelly.name = "shelly";
		shelly.attack = 0;
		shelly.health = 200;
		shelly.ruleText = "[damaged without bonus health] gain +10 bonus attack and health.";
		shelly.unique = true;
		shelly.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.dst != self)
						return false;
					if (state.boardState.combatStats[self].tempHealth > 0)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 10;
					buffState.values[ActionState::VStatBuff::tempAttack] = 10;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& goblinQueen = arr[MONSTER_IDS::GOBLIN_QUEEN];
		goblinQueen.name = "goblin queen";
		goblinQueen.attack = 0;
		goblinQueen.health = 200;
		goblinQueen.ruleText = "[start of turn] fill the board with goblins. give all goblins +3 attack. [any death] give all goblins +1 attack.";
		goblinQueen.unique = true;
		goblinQueen.tags = TAG_GOBLIN;
		goblinQueen.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 3;
					TargetOfType(info, state, buffState, self, TAG_GOBLIN, TypeTarget::all);

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
					for (uint32_t i = 0; i < BOARD_CAPACITY_PER_SIDE; ++i)
					{
						summonState.values[ActionState::VSummon::isAlly] = true;
						state.TryAddToStack(summonState);
						summonState.values[ActionState::VSummon::isAlly] = false;
						state.TryAddToStack(summonState);
					}
					return true;
				}
				if(actionState.trigger == ActionState::Trigger::onDeath)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 1;
					TargetOfType(info, state, buffState, self, TAG_GOBLIN, TypeTarget::all);
				}
				return false;
			};
		auto& masterLich = arr[MONSTER_IDS::MASTER_LICH];
		masterLich.name = "master lich";
		masterLich.attack = 3;
		masterLich.health = 300;
		masterLich.ruleText = "[end of turn] deal damage to all enemies equal to the amount of turns passed. heal that much.";
		masterLich.unique = true;
		masterLich.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = state.turn;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::health] = state.boardState.allyCount * state.turn;
					state.stack.Add() = buffState;
					return true;
				}
				return false;
			};
		auto& theCollector = arr[MONSTER_IDS::THE_COLLECTOR];
		theCollector.name = "the collector";
		theCollector.attack = 4;
		theCollector.health = 300;
		theCollector.ruleText = "[damaged] summon a random monster.";
		theCollector.unique = true;
		theCollector.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.dst != self)
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = state.GetMonster(info);
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& slimeEmperor = arr[MONSTER_IDS::SLIME_EMPEROR];
		slimeEmperor.name = "slime emperor";
		slimeEmperor.attack = 6;
		slimeEmperor.health = 300;
		slimeEmperor.ruleText = "[damaged] summon a slime with attack and health equal to the damage taken.";
		slimeEmperor.unique = true;
		slimeEmperor.tags = TAG_SLIME;
		slimeEmperor.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.dst != self)
						return false;

					const auto dmg = actionState.values[ActionState::VDamage::damage];

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = MONSTER_IDS::SLIME;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::attack)] = dmg;
					summonState.values[static_cast<uint32_t>(ActionState::VSummon::health)] = dmg;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& knifeJuggler = arr[MONSTER_IDS::KNIFE_JUGGLER];
		knifeJuggler.name = "knife juggler";
		knifeJuggler.attack = 1;
		knifeJuggler.health = 20;
		knifeJuggler.ruleText = "[draw] deal 1 damage to all enemies.";
		knifeJuggler.tags = TAG_HUMAN;
		knifeJuggler.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDraw)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& elvenSage = arr[MONSTER_IDS::ELVEN_SAGE];
		elvenSage.name = "elven sage";
		elvenSage.attack = 1;
		elvenSage.health = 16;
		elvenSage.ruleText = "[cast] summon an elf.";
		elvenSage.tags = TAG_ELF;
		elvenSage.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::ELF;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& stormElemental = arr[MONSTER_IDS::STORM_ELEMENTAL];
		stormElemental.name = "storm elemental";
		stormElemental.attack = 1;
		stormElemental.health = 20;
		stormElemental.ruleText = "[cast] +2 attack.";
		stormElemental.tags = TAG_ELEMENTAL;
		stormElemental.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 2;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& mossyElemental = arr[MONSTER_IDS::MOSSY_ELEMENTAL];
		mossyElemental.name = "mossy elemental";
		mossyElemental.attack = 1;
		mossyElemental.health = 16;
		mossyElemental.ruleText = "[damaged] +2 bonus health.";
		mossyElemental.tags = TAG_ELEMENTAL;
		mossyElemental.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& stoneElemental = arr[MONSTER_IDS::STONE_ELEMENTAL];
		stoneElemental.name = "stone elemental";
		stoneElemental.attack = 1;
		stoneElemental.health = 20;
		stoneElemental.ruleText = "[cast] +2 bonus attack and health.";
		stoneElemental.tags = TAG_ELEMENTAL;
		stoneElemental.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& goblinKing = arr[MONSTER_IDS::GOBLIN_KING];
		goblinKing.name = "goblin king";
		goblinKing.attack = 0;
		goblinKing.health = 16;
		goblinKing.ruleText = "[start of turn] summon 2 goblins.";
		goblinKing.tags = TAG_GOBLIN;
		goblinKing.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					for (uint32_t i = 0; i < 2; ++i)
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& goblinChampion = arr[MONSTER_IDS::GOBLIN_CHAMPION];
		goblinChampion.name = "goblin champion";
		goblinChampion.attack = 1;
		goblinChampion.health = 20;
		goblinChampion.ruleText = "[buffed] all other goblins gain +1 attack.";
		goblinChampion.tags = TAG_GOBLIN;
		goblinChampion.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStatBuff)
				{
					if (actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 1;
					TargetOfType(info, state, buffState, self, TAG_GOBLIN, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		auto& unstableGolem = arr[MONSTER_IDS::UNSTABLE_GOLEM];
		unstableGolem.name = "unstable golem";
		unstableGolem.attack = 3;
		unstableGolem.health = 20;
		unstableGolem.tags = TAG_TOKEN;

		auto& maidenOfTheMoon = arr[MONSTER_IDS::MAIDEN_OF_THE_MOON];
		maidenOfTheMoon.name = "moon acolyte";
		maidenOfTheMoon.attack = 1;
		maidenOfTheMoon.health = 20;
		maidenOfTheMoon.ruleText = "[any death] +2 attack and +2 bonus attack.";
		maidenOfTheMoon.tags = TAG_ELF;
		maidenOfTheMoon.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 2;
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& fleetingSoldier = arr[MONSTER_IDS::FLEETING_SOLDIER];
		fleetingSoldier.name = "berserker";
		fleetingSoldier.attack = 1;
		fleetingSoldier.health = 20;
		fleetingSoldier.ruleText = "[damaged] attack the lowest health enemy.";
		fleetingSoldier.tags = TAG_HUMAN;
		fleetingSoldier.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					const auto& boardState = state.boardState;
					if (actionState.dst != self)
						return false;

					ActionState attackState{};
					attackState.trigger = ActionState::Trigger::onAttack;
					attackState.source = ActionState::Source::board;
					attackState.src = self;
					attackState.srcUniqueId = boardState.uniqueIds[self];

					const bool allied = self < BOARD_CAPACITY_PER_SIDE;
					const uint32_t c = allied ? boardState.enemyCount : boardState.allyCount;
					if (c == 0)
						return false;

					uint32_t target = -1;
					uint32_t health = -1;
					for (uint32_t i = 0; i < c; ++i)
					{
						const uint32_t in = allied * BOARD_CAPACITY_PER_SIDE + i;
						const auto& stats = boardState.combatStats[in];
						const uint32_t h = stats.health + stats.tempHealth;
						if (h > health)
							continue;
						target = in;
						health = h;
					}

					attackState.dst = target;
					attackState.dstUniqueId = boardState.uniqueIds[target];
					state.TryAddToStack(attackState);
					return true;
				}
				return false;
			};
		auto& manaCyclone = arr[MONSTER_IDS::MANA_CYCLONE];
		manaCyclone.name = "mana cyclone";
		manaCyclone.attack = 1;
		manaCyclone.health = 20;
		manaCyclone.ruleText = "[end of turn] gain 2 attack per unspent mana.";
		manaCyclone.tags = TAG_ELEMENTAL;
		manaCyclone.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = state.mana * 2;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& woundedTroll = arr[MONSTER_IDS::WOUNDED_TROLL];
		woundedTroll.name = "wounded troll";
		woundedTroll.attack = 3;
		woundedTroll.health = 20;
		woundedTroll.ruleText = "[death] all monsters gain +5 health.";
		woundedTroll.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::health] = 5;
					TargetOfType(info, state, buffState, self, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		auto& chaosClown = arr[MONSTER_IDS::CHAOS_CLOWN];
		chaosClown.name = "mad clown";
		chaosClown.attack = 1;
		chaosClown.health = 13;
		chaosClown.ruleText = "[end of turn] dies. the opponent summons a mad clown.";
		chaosClown.tags = TAG_HUMAN;
		chaosClown.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDeath;
					killState.source = ActionState::Source::other;
					killState.dst = self;
					killState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(killState);

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::CHAOS_CLOWN;
					summonState.values[ActionState::VSummon::isAlly] = self >= BOARD_CAPACITY_PER_SIDE;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& goblinSlinger = arr[MONSTER_IDS::GOBLIN_SLINGER];
		goblinSlinger.name = "goblin slinger";
		goblinSlinger.attack = 1;
		goblinSlinger.health = 20;
		goblinSlinger.ruleText = "[ally attack] the attacked monster takes damage equal to my bonus attack.";
		goblinSlinger.tags = TAG_GOBLIN;
		goblinSlinger.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (!state.boardState.Validate(actionState, true, true))
						return false;
					if(self < BOARD_CAPACITY_PER_SIDE && actionState.src < BOARD_CAPACITY_PER_SIDE ||
						self >= BOARD_CAPACITY_PER_SIDE && actionState.src >= BOARD_CAPACITY_PER_SIDE)
					{
						const auto& combatStats = state.boardState.combatStats[self];

						ActionState damageState{};
						damageState.trigger = ActionState::Trigger::onDamage;
						damageState.source = ActionState::Source::other;
						damageState.values[ActionState::VDamage::damage] = combatStats.tempAttack;
						damageState.dst = actionState.dst;
						damageState.dstUniqueId = actionState.dstUniqueId;
						state.TryAddToStack(damageState);
						return true;
					}
				}
				return false;
			};
		auto& goblinBomb = arr[MONSTER_IDS::GOBLIN_BOMB];
		goblinBomb.name = "goblin bomber";
		goblinBomb.attack = 0;
		goblinBomb.health = 20;
		goblinBomb.ruleText = "[any attack] attacker takes damage equal to my bonus attack. +1 bonus attack.";
		goblinBomb.tags = TAG_GOBLIN;
		goblinBomb.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if(actionState.trigger == ActionState::Trigger::onAttack)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempAttack] = 1;
					state.TryAddToStack(buffState);

					const auto& stats = state.boardState.combatStats[self];

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = stats.tempAttack;
					damageState.dst = actionState.src;
					damageState.dstUniqueId = actionState.srcUniqueId;
					state.TryAddToStack(damageState);
					return true;
				}
				
				return false;
			};
		auto& goblinPartyStarter = arr[MONSTER_IDS::GOBLIN_PARTY_STARTER];
		goblinPartyStarter.name = "goblin princess";
		goblinPartyStarter.attack = 0;
		goblinPartyStarter.health = 14;
		goblinPartyStarter.ruleText = "[damaged] summon a goblin for each damage taken.";
		goblinPartyStarter.tags = TAG_GOBLIN;
		goblinPartyStarter.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (self != actionState.dst)
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;

					uint32_t dmg = actionState.values[ActionState::VDamage::damage];
					dmg = jv::Min(dmg, BOARD_CAPACITY_PER_SIDE);
					for (uint32_t i = 0; i < dmg; ++i)
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& obnoxiousFan = arr[MONSTER_IDS::OBNOXIOUS_FAN];
		obnoxiousFan.name = "lich";
		obnoxiousFan.attack = 3;
		obnoxiousFan.health = 2;
		obnoxiousFan.ruleText = "[death] if there is another allied monster, summon a lich with my attack.";
		obnoxiousFan.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (self != actionState.dst)
						return false;

					if (self < BOARD_CAPACITY_PER_SIDE && state.boardState.allyCount == 1)
						return false;
					if (self >= BOARD_CAPACITY_PER_SIDE && state.boardState.enemyCount == 1)
						return false;

					const auto& stats = state.boardState.combatStats[self];

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::OBNOXIOUS_FAN;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& slimeSoldier = arr[MONSTER_IDS::SLIME_SOLDIER];
		slimeSoldier.name = "slime soldier";
		slimeSoldier.attack = 1;
		slimeSoldier.health = 12;
		slimeSoldier.ruleText = "[end of turn] summon a slime with my stats.";
		slimeSoldier.tags = TAG_SLIME;
		slimeSoldier.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& stats = state.boardState.combatStats[self];

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::SLIME;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					summonState.values[ActionState::VSummon::health] = stats.health + stats.tempHealth;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& madPyromancer = arr[MONSTER_IDS::MAD_PYROMANCER];
		madPyromancer.name = "mad pyromancer";
		madPyromancer.attack = 1;
		madPyromancer.health = 13;
		madPyromancer.ruleText = "[cast] all other monsters take 1 damage.";
		madPyromancer.tags = TAG_HUMAN;
		madPyromancer.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		auto& phantasm = arr[MONSTER_IDS::PHANTASM];
		phantasm.name = "phantasm";
		phantasm.attack = 4;
		phantasm.health = 1;
		phantasm.ruleText = "[start of turn] +7 bonus health. [attack] bonus health becomes 0.";
		phantasm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatBuff;
					setState.source = ActionState::Source::other;
					setState.values[ActionState::VStatBuff::tempHealth] = 7;
					setState.dst = self;
					setState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(setState);
					return true;
				}
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src != self)
						return false;

					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatSet;
					setState.source = ActionState::Source::other;
					setState.values[ActionState::VStatSet::tempHealth] = 0;
					setState.dst = self;
					setState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(setState);
					return true;
				}
				return false;
			};
		auto& goblinScout = arr[MONSTER_IDS::GOBLIN_SCOUT];
		goblinScout.name = "goblin scout";
		goblinScout.attack = 1;
		goblinScout.health = 16;
		goblinScout.ruleText = "[attack] all other goblins gain my attack.";
		goblinScout.tags = TAG_GOBLIN;
		goblinScout.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src != self)
						return false;

					const auto& boardState = state.boardState;
					const auto& stats = boardState.combatStats[self];

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = stats.attack + stats.tempAttack;
					TargetOfType(info, state, buffState, self, TAG_GOBLIN, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		auto& acolyteOfPain = arr[MONSTER_IDS::ACOLYTE_OF_PAIN];
		acolyteOfPain.name = "acolyte of pain";
		acolyteOfPain.attack = 1;
		acolyteOfPain.health = 13;
		acolyteOfPain.ruleText = "[damaged] draw.";
		acolyteOfPain.tags = TAG_HUMAN;
		acolyteOfPain.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.dst != self)
						return false;
					
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& beastMaster = arr[MONSTER_IDS::BEAST_MASTER];
		beastMaster.name = "beast master";
		beastMaster.attack = 1;
		beastMaster.health = 20;
		beastMaster.ruleText = "[token attack] draw.";
		beastMaster.tags = TAG_HUMAN;
		beastMaster.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, true, true))
						return false;

					const auto& monster = info.monsters[boardState.ids[actionState.src]];
					if ((monster.tags & TAG_TOKEN) == 0)
						return false;

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& librarian = arr[MONSTER_IDS::LIBRARIAN];
		librarian.name = "librarian";
		librarian.attack = 1;
		librarian.health = 20;
		librarian.ruleText = "[draw] +1 mana. take 1 damage.";
		librarian.tags = TAG_HUMAN;
		librarian.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDraw)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					damageState.dst = self;
					damageState.dstUniqueId = state.boardState.uniqueIds[self];
					state.stack.Add() = damageState;
					++state.mana;
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, ARTIFACT_IDS::LENGTH);

		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].name = "arcane amulet";
		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].ruleText = "[start of turn] +1 mana.";
		arr[ARTIFACT_IDS::AMULET_OF_ARCANE_ACUITY].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
			{
				++state.mana;
				return true;
			}
			return false;
		};
		arr[ARTIFACT_IDS::FALSE_ARMOR].name = "repulsion armor";
		arr[ARTIFACT_IDS::FALSE_ARMOR].ruleText = "[attack] the attacked monster takes damage equal to my bonus health.";
		arr[ARTIFACT_IDS::FALSE_ARMOR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src != self)
						return false;

					const uint32_t dmg = state.boardState.combatStats[self].tempHealth;
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = dmg;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					state.stack.Add() = damageState;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::REVERSE_CARD].name = "reverse card";
		arr[ARTIFACT_IDS::REVERSE_CARD].ruleText = "[damaged] gain that much attack.";
		arr[ARTIFACT_IDS::REVERSE_CARD].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					const auto& boardState = state.boardState;
					if (actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = actionState.values[ActionState::VDamage::damage];
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::THORNMAIL].name = "thornmail";
		arr[ARTIFACT_IDS::THORNMAIL].ruleText = "[attacked] the attacker takes damage equal to their attack.";
		arr[ARTIFACT_IDS::THORNMAIL].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.dst != self)
						return false;

					const auto& stats = state.boardState.combatStats[actionState.src];

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.src;
					damageState.dstUniqueId = actionState.srcUniqueId;
					damageState.values[ActionState::VDamage::damage] = stats.attack + stats.tempAttack;
					state.TryAddToStack(damageState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MAGE_ARMOR].name = "mage armor";
		arr[ARTIFACT_IDS::MAGE_ARMOR].ruleText = "[cast] +2 bonus health.";
		arr[ARTIFACT_IDS::MAGE_ARMOR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& boardState = state.boardState;
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].name = "mask of youth";
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].ruleText = "[bonus attack buffed] gain that much bonus health.";
		arr[ARTIFACT_IDS::MASK_OF_ETERNAL_YOUTH].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStatBuff)
				{
					if (actionState.dst != self)
						return false;

					const uint32_t bonusAtk = actionState.values[ActionState::VStatBuff::tempAttack];
					if (bonusAtk == -1)
						return false;

					const auto& boardState = state.boardState;
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempHealth] = bonusAtk;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].name = "corrupting knife";
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].ruleText = "[end of turn] +4 attack. take 1 damage.";
		arr[ARTIFACT_IDS::CORRUPTING_KNIFE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = self;
					damageState.dstUniqueId = boardState.uniqueIds[self];
					damageState.values[ActionState::VDamage::damage] = 1;
					state.TryAddToStack(damageState);

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 4;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].name = "the brand";
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].ruleText = "[start of turn] die. all allies gain 5 health.";
		arr[ARTIFACT_IDS::SACRIFICIAL_ALTAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					const auto& boardState = state.boardState;

					ActionState deathState{};
					deathState.trigger = ActionState::Trigger::onDeath;
					deathState.source = ActionState::Source::other;
					deathState.dst = self;
					deathState.dstUniqueId = boardState.uniqueIds[self];
					state.TryAddToStack(deathState);

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::health] = 5;
					TargetOfType(info, state, buffState, self, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLOOD_AXE].name = "bloodied axe";
		arr[ARTIFACT_IDS::BLOOD_AXE].ruleText = "[kill] +5 attack.";
		arr[ARTIFACT_IDS::BLOOD_AXE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;

					if (actionState.source != ActionState::Source::board)
						return false;
					if (actionState.src != self)
						return false;
					if (!boardState.Validate(actionState, true, false))
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 5;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_CLAW].name = "rusty claw";
		arr[ARTIFACT_IDS::RUSTY_CLAW].ruleText = "[any death] draw.";
		arr[ARTIFACT_IDS::RUSTY_CLAW].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLOOD_HAMMER].name = "blood hammer";
		arr[ARTIFACT_IDS::BLOOD_HAMMER].ruleText = "[any death] +4 bonus attack.";
		arr[ARTIFACT_IDS::BLOOD_HAMMER].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempAttack] = 4;
					state.TryAddToStack(buffState);
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
					if (actionState.dst != self)
						return false;
					if (!boardState.Validate(actionState, false, true))
						return false;

					state.tapped[self] = false;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BOOTS_OF_SWIFTNESS].name = "cup of blood";
		arr[ARTIFACT_IDS::BOOTS_OF_SWIFTNESS].ruleText = "[ally death] untap.";
		arr[ARTIFACT_IDS::BOOTS_OF_SWIFTNESS].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					if (actionState.dst >= BOARD_CAPACITY_PER_SIDE)
						return false;
					state.tapped[self] = false;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::BLESSED_RING].name = "blessed ring";
		arr[ARTIFACT_IDS::BLESSED_RING].ruleText = "[attack] draw.";
		arr[ARTIFACT_IDS::BLESSED_RING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src != self)
						return false;
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
				}
				return false;
			};
		arr[ARTIFACT_IDS::MANAMUNE].name = "manamune";
		arr[ARTIFACT_IDS::MANAMUNE].ruleText = "[cast] +2 bonus attack.";
		arr[ARTIFACT_IDS::MANAMUNE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& boardState = state.boardState;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::THORN_WHIP].name = "thorn whip";
		arr[ARTIFACT_IDS::THORN_WHIP].ruleText = "[attack] deal 1 damage to all other monsters.";
		arr[ARTIFACT_IDS::THORN_WHIP].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.src != self)
						return false;
					
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					TargetOfType(info, state, damageState, self, -1, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RED_CLOTH].name = "red cloth";
		arr[ARTIFACT_IDS::RED_CLOTH].ruleText = "[end of turn] gain 3 bonus health for every enemy. they all attack me.";
		arr[ARTIFACT_IDS::RED_CLOTH].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;

					ActionState attackState{};
					attackState.trigger = ActionState::Trigger::onAttack;
					attackState.source = ActionState::Source::board;
					attackState.dst = self;
					attackState.dstUniqueId = boardState.uniqueIds[self];
					
					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						attackState.src = BOARD_CAPACITY_PER_SIDE + i;
						attackState.srcUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
						state.stack.Add() = attackState;
					}

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempHealth] = 3 * boardState.enemyCount;
					state.TryAddToStack(buffState);

					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].name = "helmet of haste";
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].ruleText = "[attack] all allies gain +1 attack.";
		arr[ARTIFACT_IDS::HELMET_OF_THE_HOST].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if (actionState.src != self)
						return false;
					if (!boardState.Validate(actionState, true, false))
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 1;
					TargetOfType(info, state, buffState, self, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_SPEAR].name = "moaning orb";
		arr[ARTIFACT_IDS::RUSTY_SPEAR].ruleText = "[any death] +1 mana.";
		arr[ARTIFACT_IDS::RUSTY_SPEAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					++state.mana;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::RUSTY_COLLAR].name = "rusty collar";
		arr[ARTIFACT_IDS::RUSTY_COLLAR].ruleText = "[on bonus attack buffed] gain that much attack.";
		arr[ARTIFACT_IDS::RUSTY_COLLAR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStatBuff)
				{
					if (actionState.dst != self)
						return false;

					const uint32_t bonusAtk = actionState.values[ActionState::VStatBuff::tempAttack];
					if (bonusAtk == -1)
						return false;

					const auto& boardState = state.boardState;
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = bonusAtk;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::SWORD_OF_SPELLCASTING].name = "mage sword";
		arr[ARTIFACT_IDS::SWORD_OF_SPELLCASTING].ruleText = "[non combat damage to any target] +1 attack.";
		arr[ARTIFACT_IDS::SWORD_OF_SPELLCASTING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.source == ActionState::Source::board)
						return false;
					
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 1;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::STAFF_OF_AEONS].name = "staff of aeons";
		arr[ARTIFACT_IDS::STAFF_OF_AEONS].ruleText = "[2 or more non combat damage to any target] +1 mana.";
		arr[ARTIFACT_IDS::STAFF_OF_AEONS].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDamage)
				{
					if (actionState.source == ActionState::Source::board)
						return false;
					if (actionState.values[ActionState::VDamage::damage] < 2)
						return false;
					++state.mana;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::STAFF_OF_SUMMONING].name = "demonic staff";
		arr[ARTIFACT_IDS::STAFF_OF_SUMMONING].ruleText = "[cast 2 cost or higher] summon a 6/6 demon.";
		arr[ARTIFACT_IDS::STAFF_OF_SUMMONING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& card = info.spells[state.hand[actionState.src]];
					if (card.cost < 2)
						return false;

					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::DEMON;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::health] = 6;
					summonState.values[ActionState::VSummon::attack] = 6;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MANA_RING].name = "mana ring";
		arr[ARTIFACT_IDS::MANA_RING].ruleText = "[cast 2 cost or higher] draw.";
		arr[ARTIFACT_IDS::MANA_RING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast)
				{
					const auto& card = info.spells[state.hand[actionState.src]];
					if (card.cost < 2)
						return false;

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		return arr;
	}

	jv::Array<uint32_t> CardGame::GetBossCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<uint32_t>(arena, 12);
		for (auto& a : arr)
			a = -1;
		arr[0] = MONSTER_IDS::GREAT_TROLL;
		arr[1] = MONSTER_IDS::SLIME_QUEEN;
		arr[2] = MONSTER_IDS::LICH_KING;
		arr[3] = MONSTER_IDS::MIRROR_KNIGHT;
		arr[4] = MONSTER_IDS::BOMBA;
		arr[5] = MONSTER_IDS::THE_DREAD;
		arr[6] = MONSTER_IDS::ARCHMAGE;
		arr[7] = MONSTER_IDS::SHELLY;
		arr[8] = MONSTER_IDS::GOBLIN_QUEEN;
		arr[9] = MONSTER_IDS::MASTER_LICH;
		arr[10] = MONSTER_IDS::THE_COLLECTOR;
		arr[11] = MONSTER_IDS::SLIME_EMPEROR;
		return arr;
	}

	jv::Array<RoomCard> CardGame::GetRoomCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<RoomCard>(arena, ROOM_IDS::LENGTH);
		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = c++;

		auto& fieldOfVengeance = arr[ROOM_IDS::FIELD_OF_VENGEANCE];
		fieldOfVengeance.name = "field of violence";
		fieldOfVengeance.ruleText = "[monster is dealt non combat damage] attack randomly.";
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
				state.TryAddToStack(attackState);
				return true;
			}
			return false;
		};
		auto& forsakenBattlefield = arr[ROOM_IDS::FORSAKEN_BATTLEFIELD];
		forsakenBattlefield.name = "battlefield";
		forsakenBattlefield.ruleText = "[any non token death] fill the opponents board with vultures.";
		forsakenBattlefield.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
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
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& blessedHalls = arr[ROOM_IDS::BLESSED_HALLS];
		blessedHalls.name = "blessed halls";
		blessedHalls.ruleText = "[start of turn] all enemies gain +4 bonus health.";
		blessedHalls.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 4;

					TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& khaalsDomain = arr[ROOM_IDS::KHAALS_DOMAIN];
		khaalsDomain.name = "khaals domain";
		khaalsDomain.ruleText = "[end of turn] all monsters take 1 damage.";
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
		arenaOfTheDamned.name = "culling grounds";
		arenaOfTheDamned.ruleText = "[end of turn] all monsters with the lowest health take 5 damage.";
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
						const auto& stats = boardState.combatStats[i];
						const auto health = stats.health + stats.tempHealth;
						if (health < lowestHealth)
							lowestHealth = health;
					}
					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						const auto& stats = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i];
						const auto health = stats.health + stats.tempHealth;
						if (health < lowestHealth)
							lowestHealth = health;
					}

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 5;

					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto health = boardState.combatStats[i].health;
						if (health == lowestHealth)
						{
							damageState.dst = i;
							damageState.dstUniqueId = boardState.uniqueIds[i];
							state.TryAddToStack(damageState);
						}
					}

					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						const auto health = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i].health;
						if (health == lowestHealth)
						{
							damageState.dst = BOARD_CAPACITY_PER_SIDE + i;
							damageState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
							state.TryAddToStack(damageState);
						}
					}

					return true;
				}
				return false;
			};
		auto& tranquilWaters = arr[ROOM_IDS::TRANQUIL_WATERS];
		tranquilWaters.name = "tranquil waters";
		tranquilWaters.ruleText = "[start of turn] the attack of all monsters becomes 1.";
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
		prisonOfEternity.name = "eternity prison";
		prisonOfEternity.ruleText = "on turn 6, kill all monsters.";
		prisonOfEternity.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.turn != 6)
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
		arcaneIntellect.cost = 2;
		arcaneIntellect.type = SpellCard::Type::all;
		arcaneIntellect.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
			{
				ActionState drawState{};
				drawState.trigger = ActionState::Trigger::onDraw;
				drawState.source = ActionState::Source::other;
				for (uint32_t i = 0; i < 2; ++i)
					state.TryAddToStack(drawState);
				return true;
			}
			return false;
		};
		auto& dreadSacrifice = arr[SPELL_IDS::DREAD_SACRIFICE];
		dreadSacrifice.name = "sacrifice";
		dreadSacrifice.ruleText = "for every allied token, gain 2 mana. then kill them.";
		dreadSacrifice.cost = 1;
		dreadSacrifice.type = SpellCard::Type::all;
		dreadSacrifice.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState killState{};
					killState.trigger = ActionState::Trigger::onDeath;
					killState.source = ActionState::Source::other;
					TargetOfType(info, state, killState, 0, TAG_TOKEN, TypeTarget::allies);

					const auto& boardState = state.boardState;

					uint32_t c = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						if ((info.monsters[boardState.ids[i]].tags & TAG_TOKEN) == 0)
							continue;
						++c;
					}

					state.mana += c * 2;
					return true;
				}
				return false;
			};
		auto& tranquilize = arr[SPELL_IDS::TRANQUILIZE];
		tranquilize.name = "tranquilize";
		tranquilize.ruleText = "set my attack to 1.";
		tranquilize.cost = 3;
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
					state.TryAddToStack(setState);
					return true;
				}
				return false;
			};
		auto& doubleTrouble = arr[SPELL_IDS::DOUBLE_TROUBLE];
		doubleTrouble.name = "double trouble";
		doubleTrouble.ruleText = "double my attack.";
		doubleTrouble.cost = 3;
		doubleTrouble.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& stats = state.boardState.combatStats[actionState.dst];

					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatBuff;
					setState.source = ActionState::Source::other;
					setState.dst = actionState.dst;
					setState.dstUniqueId = actionState.dstUniqueId;
					setState.values[ActionState::VStatBuff::attack] = stats.attack + stats.tempAttack;
					state.TryAddToStack(setState);
					return true;
				}
				return false;
			};
		auto& goblinAmbush = arr[SPELL_IDS::GOBLIN_AMBUSH];
		goblinAmbush.name = "goblin ambush";
		goblinAmbush.ruleText = "summon 2 goblins.";
		goblinAmbush.cost = 2;
		goblinAmbush.type = SpellCard::Type::all;
		goblinAmbush.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					for (uint32_t i = 0; i < 2; ++i)
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& windfall = arr[SPELL_IDS::WINDFALL];
		windfall.name = "windfall";
		windfall.ruleText = "all enemies take 2 damage.";
		windfall.cost = 2;
		windfall.type = SpellCard::Type::all;
		windfall.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 2;
					TargetOfType(info, state, damageState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& rally = arr[SPELL_IDS::RALLY];
		rally.name = "rally";
		rally.ruleText = "all allies gain +1 attack.";
		rally.cost = 2;
		rally.type = SpellCard::Type::all;
		rally.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 1;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& holdTheLine = arr[SPELL_IDS::HOLD_THE_LINE];
		holdTheLine.name = "hold the line";
		holdTheLine.ruleText = "all allies gain +2 bonus health.";
		holdTheLine.cost = 2;
		holdTheLine.type = SpellCard::Type::all;
		holdTheLine.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& rampantGrowth = arr[SPELL_IDS::RAMPANT_GROWTH];
		rampantGrowth.name = "rampant growth";
		rampantGrowth.ruleText = "+1 max mana.";
		rampantGrowth.cost = 1;
		rampantGrowth.type = SpellCard::Type::all;
		rampantGrowth.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					++state.maxMana;
					return true;
				}
				return false;
			};
		auto& enlightenment = arr[SPELL_IDS::ENLIGHTENMENT];
		enlightenment.name = "enlightenment";
		enlightenment.ruleText = "draw until your hand is full.";
		enlightenment.cost = 4;
		enlightenment.type = SpellCard::Type::all;
		enlightenment.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					for (uint32_t i = 0; i < HAND_MAX_SIZE - state.hand.count; ++i)
						state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& druidicRitual = arr[SPELL_IDS::DRUIDIC_RITUAL];
		druidicRitual.name = "druidic ritual";
		druidicRitual.ruleText = "summon 2 elves.";
		druidicRitual.cost = 1;
		druidicRitual.type = SpellCard::Type::all;
		druidicRitual.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 1;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::ELF;
					for (uint32_t i = 0; i < 2; ++i)
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& ascension = arr[SPELL_IDS::ASCENSION];
		ascension.name = "ascension";
		ascension.ruleText = "+6 attack.";
		ascension.cost = 3;
		ascension.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					buffState.values[ActionState::VStatBuff::attack] = 6;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& stampede = arr[SPELL_IDS::STAMPEDE];
		stampede.name = "stampede";
		stampede.ruleText = "all allies gain +3 attack.";
		stampede.cost = 3;
		stampede.type = SpellCard::Type::all;
		stampede.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 3;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& enrage = arr[SPELL_IDS::ENRAGE];
		enrage.name = "enrage";
		enrage.ruleText = "+2 bonus attack.";
		enrage.cost = 1;
		enrage.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& protect = arr[SPELL_IDS::PROTECT];
		protect.name = "protect";
		protect.ruleText = "+3 bonus health.";
		protect.cost = 1;
		protect.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 3;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& shield = arr[SPELL_IDS::SHIELD];
		shield.name = "shield";
		shield.ruleText = "+5 bonus health. draw.";
		shield.cost = 2;
		shield.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 5;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& groupHug = arr[SPELL_IDS::GROUP_HUG];
		groupHug.name = "group hug";
		groupHug.ruleText = "all allies gain bonus health equal to my bonus health.";
		groupHug.cost = 1;
		groupHug.type = SpellCard::Type::target;
		groupHug.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if (!state.boardState.Validate(actionState, false, true))
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = state.boardState.combatStats[actionState.dst].tempHealth;
					TargetOfType(info, state, buffState, 0, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& stall = arr[SPELL_IDS::STALL];
		stall.name = "stall";
		stall.ruleText = "all monsters gain +3 bonus health.";
		stall.cost = 2;
		stall.type = SpellCard::Type::all;
		stall.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 3;
					TargetOfType(info, state, buffState, -1, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		auto& balance = arr[SPELL_IDS::BALANCE];
		balance.name = "balance";
		balance.ruleText = "gain bonus health equal to my bonus attack.";
		balance.cost = 1;
		balance.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if (!state.boardState.Validate(actionState, false, true))
						return false;

					const auto& stats = state.boardState.combatStats[actionState.dst];

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = stats.tempAttack;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& counterBalance = arr[SPELL_IDS::COUNTER_BALANCE];
		counterBalance.name = "counter balance";
		counterBalance.ruleText = "gain bonus attack equal to my bonus health.";
		counterBalance.cost = 1;
		counterBalance.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if (!state.boardState.Validate(actionState, false, true))
						return false;

					const auto& stats = state.boardState.combatStats[actionState.dst];

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = stats.tempHealth;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& grit = arr[SPELL_IDS::GRIT];
		grit.name = "grit";
		grit.ruleText = "gain attack equal to my bonus attack.";
		grit.cost = 1;
		grit.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if (!state.boardState.Validate(actionState, false, true))
						return false;

					const auto& stats = state.boardState.combatStats[actionState.dst];

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = stats.tempAttack;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& infuriate = arr[SPELL_IDS::INFURIATE];
		infuriate.name = "infuriate";
		infuriate.ruleText = "+3 attack.";
		infuriate.cost = 2;
		infuriate.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::attack] = 3;
					buffState.dst = actionState.dst;
					buffState.dstUniqueId = actionState.dstUniqueId;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		auto& goblinChant = arr[SPELL_IDS::GOBLIN_CHANT];
		goblinChant.name = "goblin chant";
		goblinChant.ruleText = "+1 mana for each friendly goblin.";
		goblinChant.cost = 1;
		goblinChant.type = SpellCard::Type::all;
		goblinChant.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					uint32_t m = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto& monster = info.monsters[boardState.ids[i]];
						m += (monster.tags & TAG_GOBLIN) != 0;
					}
					state.mana += m;
					return true;
				}
				return false;
			};
		auto& treasureHunt = arr[SPELL_IDS::TREASURE_HUNT];
		treasureHunt.name = "treasure hunt";
		treasureHunt.ruleText = "summon 3 treasures for your opponent.";
		treasureHunt.cost = 1;
		treasureHunt.type = SpellCard::Type::all;
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
						state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& rewind = arr[SPELL_IDS::REWIND];
		rewind.name = "rewind";
		rewind.ruleText = "untap all allies.";
		rewind.cost = 4;
		rewind.type = SpellCard::Type::all;
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
		manaSurge.ruleText = "+4 mana.";
		manaSurge.cost = 2;
		manaSurge.type = SpellCard::Type::all;
		manaSurge.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					state.mana += 4;
					return true;
				}
				return false;
			};
		auto& darkRitual = arr[SPELL_IDS::DARK_RITUAL];
		darkRitual.name = "dark ritual";
		darkRitual.ruleText = "+2 mana.";
		darkRitual.cost = 1;
		darkRitual.type = SpellCard::Type::all;
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
		flameBolt.ruleText = "deal 6 damage.";
		flameBolt.cost = 2;
		flameBolt.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 6;
					state.TryAddToStack(damageState);
					return true;
				}
				return false;
			};
		auto& pyroblast = arr[SPELL_IDS::PYROBlAST];
		pyroblast.name = "pyro blast";
		pyroblast.ruleText = "deal 10 damage.";
		pyroblast.cost = 3;
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
					state.TryAddToStack(damageState);
					return true;
				}
				return false;
			};
		auto& shock = arr[SPELL_IDS::SHOCK];
		shock.name = "shock";
		shock.ruleText = "deal 3 damage.";
		shock.cost = 1;
		shock.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 3;
					state.TryAddToStack(damageState);
					return true;
				}
				return false;
			};
		auto& unstableCopy = arr[SPELL_IDS::UNSTABLE_COPY];
		unstableCopy.name = "unstable copy";
		unstableCopy.ruleText = "summon a demon with my attack and 1 health.";
		unstableCopy.cost = 1;
		unstableCopy.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					summonState.values[ActionState::VSummon::health] = 1;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::DEMON;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& perfectCopy = arr[SPELL_IDS::PERFECT_COPY];
		perfectCopy.name = "perfect copy";
		perfectCopy.ruleText = "summon a demon with my stats.";
		perfectCopy.cost = 4;
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
					summonState.values[ActionState::VSummon::health] = stats.health + stats.tempHealth;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::DEMON;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		auto& painForGain = arr[SPELL_IDS::PAIN_FOR_GAIN];
		painForGain.name = "pain for gain";
		painForGain.ruleText = "deal 3 damage. [ally] draw 3.";
		painForGain.cost = 2;
		painForGain.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					if(actionState.dst < BOARD_CAPACITY_PER_SIDE)
					{
						ActionState drawState{};
						drawState.trigger = ActionState::Trigger::onDraw;
						drawState.source = ActionState::Source::other;
						for (uint32_t i = 0; i < 3; ++i)
							state.TryAddToStack(drawState);
					}

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 3;
					state.TryAddToStack(damageState);
					
					return true;
				}
				return false;
			};
		auto& betrayal = arr[SPELL_IDS::BETRAYAL];
		betrayal.name = "betrayal";
		betrayal.ruleText = "my allies take damage equal to my attack.";
		betrayal.cost = 2;
		betrayal.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					const auto& stats = boardState.combatStats[actionState.dst];
					damageState.values[ActionState::VDamage::damage] = stats.attack + stats.tempAttack;
					TargetOfType(info, state, damageState, actionState.dst, -1, TypeTarget::allies);
					return true;
				}
				return false;
			};
		auto& incantationOfDoom = arr[SPELL_IDS::INCANTATION_OF_DOOM];
		incantationOfDoom.name = "doom";
		incantationOfDoom.ruleText = "all monsters take 10 damage.";
		incantationOfDoom.cost = 5;
		incantationOfDoom.type = SpellCard::Type::all;
		incantationOfDoom.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 10;
					TargetOfType(info, state, damageState, 0, -1, TypeTarget::all);
					return true;
				}
				return false;
			};
		auto& pariah = arr[SPELL_IDS::PARIAH];
		pariah.name = "pariah";
		pariah.ruleText = "[ally] becomes the target of all enemies.";
		pariah.cost = 3;
		pariah.type = SpellCard::Type::target;
		pariah.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;
					if (actionState.dst >= BOARD_CAPACITY_PER_SIDE)
						return false;
					for (auto& target : state.targets)
						target = actionState.dst;
					return true;
				}
				return false;
			};
		auto& fireStorm = arr[SPELL_IDS::FIRE_STORM];
		fireStorm.name = "fire storm";
		fireStorm.ruleText = "all enemies take 4 damage.";
		fireStorm.cost = 3;
		fireStorm.type = SpellCard::Type::all;
		fireStorm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 4;
					TargetOfType(info, state, damageState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& unity = arr[SPELL_IDS::UNITY];
		unity.name = "unity";
		unity.ruleText = "all other allies with my tags gain my bonus attack.";
		unity.cost = 1;
		unity.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;
					const auto& stats = boardState.combatStats[actionState.dst];

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempAttack] = stats.tempAttack;
					TargetOfType(info, state, buffState, actionState.dst, info.monsters[boardState.ids[actionState.dst]].tags, TypeTarget::allies, true);
					return true;
				}
				return false;
			};
		auto& tribalism = arr[SPELL_IDS::TRIBALISM];
		tribalism.name = "tribalism";
		tribalism.ruleText = "gain +1 mana for every ally that shares a tag with me.";
		tribalism.cost = 1;
		tribalism.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;
					const auto tags = info.monsters[boardState.ids[actionState.dst]].tags;

					uint32_t c = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						if (i == self)
							continue;
						const auto otherTags = info.monsters[boardState.ids[i]].tags;
						if (otherTags & tags)
							++c;
					}
					state.mana += c;
					return true;
				}
				return false;
			};
		auto& loyalWorkforce = arr[SPELL_IDS::LOYAL_WORKFORCE];
		loyalWorkforce.name = "loyal workforce";
		loyalWorkforce.ruleText = "draw for each allied token.";
		loyalWorkforce.cost = 1;
		loyalWorkforce.type = SpellCard::Type::all;
		loyalWorkforce.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;

					uint32_t c = 0;
					for (uint32_t i = 0; i < boardState.allyCount; ++i)
					{
						const auto otherTags = info.monsters[boardState.ids[i]].tags;
						if (otherTags & TAG_TOKEN)
							++c;
					}

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					for (uint32_t i = 0; i < c; ++i)
						state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& pick = arr[SPELL_IDS::PICK];
		pick.name = "pick";
		pick.ruleText = "deal 2 damage and draw.";
		pick.cost = 1;
		pick.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, false, true))
						return false;

					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = state.boardState.uniqueIds[actionState.dst];
					damageState.values[ActionState::VDamage::damage] = 2;
					state.TryAddToStack(damageState);

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		auto& cycle = arr[SPELL_IDS::CYCLE];
		cycle.name = "cycle";
		cycle.ruleText = "draw. +1 mana.";
		cycle.cost = 1;
		cycle.type = SpellCard::Type::all;
		cycle.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);

					++state.mana;
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

		arr[CURSE_IDS::FADING].name = "fading";
		arr[CURSE_IDS::FADING].ruleText = "[end of turn] take one damage.";
		arr[CURSE_IDS::FADING].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = self;
					damageState.dstUniqueId = state.boardState.uniqueIds[self];
					damageState.values[ActionState::VDamage::damage] = 1;
					state.TryAddToStack(damageState);
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::WEAKNESS].name = "weakness";
		arr[CURSE_IDS::WEAKNESS].ruleText = "[start of turn] my attack becomes 1.";
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
					state.TryAddToStack(setState);
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::COWARDICE].name = "cowardice";
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
		arr[CURSE_IDS::DUM_DUM].name = "dum dum";
		arr[CURSE_IDS::DUM_DUM].ruleText = "[start of turn] your maximum mana is capped at 3.";
		arr[CURSE_IDS::DUM_DUM].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.maxMana <= 3)
						return false;
					state.maxMana = 3;
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::HATE].name = "hate";
		arr[CURSE_IDS::HATE].ruleText = "[end of turn] +2 attack, take 2 damage.";
		arr[CURSE_IDS::HATE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState dmgState{};
					dmgState.trigger = ActionState::Trigger::onDamage;
					dmgState.source = ActionState::Source::other;
					dmgState.dst = self;
					dmgState.dstUniqueId = state.boardState.uniqueIds[self];
					dmgState.values[ActionState::VDamage::damage] = 2;
					state.TryAddToStack(dmgState);

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = state.boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::attack] = 2;
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::HAUNTING].name = "haunting";
		arr[CURSE_IDS::HAUNTING].ruleText = "[attack] summon a goblin for your opponent.";
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
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::TIME].name = "time";
		arr[CURSE_IDS::TIME].ruleText = "[start of turn 6] die.";
		arr[CURSE_IDS::TIME].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if (state.turn != 6)
						return false;

					ActionState deathState{};
					deathState.trigger = ActionState::Trigger::onDeath;
					deathState.source = ActionState::Source::other;
					deathState.dst = self;
					deathState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(deathState);
					return true;
				}
				return false;
			};
		arr[CURSE_IDS::VULNERABILITY].name = "vulnerability";
		arr[CURSE_IDS::VULNERABILITY].ruleText = "[attacked] take 1 damage.";
		arr[CURSE_IDS::VULNERABILITY].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.dst != self)
						return false;

					ActionState dmgState{};
					dmgState.trigger = ActionState::Trigger::onDamage;
					dmgState.source = ActionState::Source::other;
					dmgState.dst = self;
					dmgState.dstUniqueId = state.boardState.uniqueIds[self];
					dmgState.values[ActionState::VDamage::damage] = 1;
					state.TryAddToStack(dmgState);
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
		aetherSurge.ruleText = "[cast] all enemies gain 1 attack and health.";
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
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};

		auto& gatheringStorm = arr[EVENT_IDS::GATHERING_STORM];
		gatheringStorm.name = "gathering storm";
		gatheringStorm.ruleText = "[start of turn] all enemies gain +2 attack.";
		gatheringStorm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::attack] = 2;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			};

		auto& noYou = arr[EVENT_IDS::NO_YOU];
		noYou.name = "repercussion";
		noYou.ruleText = "[enemy attacked] attack back.";
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
				state.TryAddToStack(attackState);
				return true;
			};
		auto& goblinPlague = arr[EVENT_IDS::GOBLIN_PLAGUE];
		goblinPlague.name = "goblin plague";
		goblinPlague.ruleText = "[end of turn] summon 4 goblins.";
		goblinPlague.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onEndOfTurn)
					return false;

				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.source = ActionState::Source::other;
				summonState.values[ActionState::VSummon::isAlly] = 0;
				summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
				for (uint32_t i = 0; i < 4; ++i)
					state.TryAddToStack(summonState);
				return true;
			};
		auto& whirlwind = arr[EVENT_IDS::WHIRLWIND];
		whirlwind.name = "whirlwind";
		whirlwind.ruleText = "[end of turn] all monsters take 1 damage.";
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
		healingWord.ruleText = "[start of turn] all enemies gain +4 bonus health.";
		healingWord.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::tempHealth] = 4;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			};

		auto& briefRespite = arr[EVENT_IDS::BRIEF_RESPISE];
		briefRespite.name = "brief respite";

		auto& chaseTheDragon = arr[EVENT_IDS::CHASE_THE_DRAGON];
		chaseTheDragon.name = "chase the dragon";
		chaseTheDragon.ruleText = "[ally attack] all enemies target the attacker.";
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
		inputState.esc.change = false;
		
		for (const auto& callback : mouseCallbacks)
		{
			SetInputState(inputState.lMouse, GLFW_MOUSE_BUTTON_LEFT, callback);
			SetInputState(inputState.rMouse, GLFW_MOUSE_BUTTON_RIGHT, callback);
		}
		for (const auto& callback : keyCallbacks)
		{
			SetInputState(inputState.enter, GLFW_KEY_SPACE, callback);
			SetInputState(inputState.esc, GLFW_KEY_ESCAPE, callback);
		}
		
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

	bool Update(bool& outRestart)
	{
		assert(cardGameRunning);
		const auto res =  cardGame.Update();
		outRestart = cardGame.restart;
		return res;
	}

	void Stop()
	{
		assert(cardGameRunning);
		cardGame.Destroy(cardGame);
		cardGame = {};
		cardGameRunning = false;
	}
}
