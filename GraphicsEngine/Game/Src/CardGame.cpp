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
#include "miniaudio.h"

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
		TaskSystem<RenderTask>* frontRenderTasks;
		TaskSystem<TextTask>* textTasks;
		TaskSystem<PixelPerfectRenderTask>* pixelPerfectRenderTasks;
		TaskSystem<LightTask>* lightTasks;
		InstancedRenderInterpreter<RenderTask>* renderInterpreter;
		InstancedRenderInterpreter<RenderTask>* priorityRenderInterpreter;
		InstancedRenderInterpreter<RenderTask>* frontRenderInterpreter;
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
		TextureStreamer largeTextureStreamer;
		float pixelation = 1;

		bool activePlayer;
		bool inCombat;
		float p1Lerp, p2Lerp;
		bool restart;

		ma_engine audioEngine;
		ma_sound audioBackground;

		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(CardGame& cardGame);

		[[nodiscard]] static jv::Array<const char*> GetTexturePaths(jv::Arena& arena);
		[[nodiscard]] static jv::Array<const char*> GetDynamicTexturePaths(jv::Arena& arena, jv::Arena& frameArena);
		[[nodiscard]] static jv::Array<const char*> GetDynamicBossTexturePaths(jv::Arena& arena, jv::Arena& frameArena);
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
			constexpr glm::ivec2 res = SIMULATED_RESOLUTION * 3;
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
		glm::ivec2 requestedResolution{};
		
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
			requestedResolution,
			inputState,
			*textTasks,
			*pixelPerfectRenderTasks,
			*lightTasks,
			dt,
			textureStreamer,
			largeTextureStreamer,
			screenShakeInfo,
			pixelation,
			activePlayer,
			inCombat,
			fullscreen,
			cardGame.audioEngine,
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
		if(requestedResolution != glm::ivec2(0))
		{
			SaveResolution(requestedResolution, false);
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
			engineCreateInfo.name = "DARK CRESCENT";
			engineCreateInfo.icon = "Art/icon.png";
			outCardGame->engine = Engine::Create(engineCreateInfo);
		}
		outCardGame->restart = false;

		auto engineConfig = ma_engine_config_init();
		auto result = ma_engine_init(nullptr, &outCardGame->audioEngine);
		assert(result == MA_SUCCESS);

		result = ma_sound_init_from_file(&outCardGame->audioEngine, SOUND_BACKGROUND_MUSIC,
			MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_STREAM, nullptr, nullptr, &outCardGame->audioBackground);
		assert(result == MA_SUCCESS);
		ma_sound_set_looping(&outCardGame->audioBackground, true);
		ma_sound_start(&outCardGame->audioBackground);

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
			outCardGame->frontRenderTasks = &outCardGame->engine.AddTaskSystem<RenderTask>();
			outCardGame->frontRenderTasks->Allocate(outCardGame->arena, 8);
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

			outCardGame->frontRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter<RenderTask>>(
				*outCardGame->frontRenderTasks, createInfo);
			outCardGame->frontRenderInterpreter->Enable(enableInfo);
			outCardGame->frontRenderInterpreter->image = outCardGame->atlas;

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
			pixelPerfectRenderInterpreterCreateInfo.frontRenderTasks = outCardGame->frontRenderTasks;
			pixelPerfectRenderInterpreterCreateInfo.screenShakeInfo = &outCardGame->screenShakeInfo;

			pixelPerfectRenderInterpreterCreateInfo.resolution = SIMULATED_RESOLUTION;
			pixelPerfectRenderInterpreterCreateInfo.simulatedResolution = SIMULATED_RESOLUTION;
			pixelPerfectRenderInterpreterCreateInfo.background = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;

			outCardGame->pixelPerfectRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<PixelPerfectRenderTask, PixelPerfectRenderInterpreter>(
				*outCardGame->pixelPerfectRenderTasks, pixelPerfectRenderInterpreterCreateInfo);

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.alphabetAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.largeAlphabetAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::largeAlphabet)];
			textInterpreterCreateInfo.symbolAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.largeNumberAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::largeNumbers)];
			textInterpreterCreateInfo.textBubbleAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::textBubble)];
			textInterpreterCreateInfo.textBubbleTailAtlasTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::textBubbleTail)];
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
		imageCreateInfo.resolution = CARD_ART_SHAPE * glm::ivec2(CARD_ART_MAX_LENGTH, 1);
		imageCreateInfo.scene = outCardGame->scene;

		outCardGame->textureStreamer = TextureStreamer::Create(outCardGame->arena, 32, 256, imageCreateInfo, CARD_ART_SHAPE.x);
		const auto dynTexts = GetDynamicTexturePaths(outCardGame->engine.GetMemory().arena, outCardGame->engine.GetMemory().frameArena);
		for (const auto& dynText : dynTexts)
			outCardGame->textureStreamer.DefineTexturePath(dynText);
		
		imageCreateInfo.resolution = LARGE_CARD_ART_SHAPE * glm::ivec2(LARGE_CARD_ART_MAX_LENGTH, 1);
		imageCreateInfo.scene = outCardGame->scene;

		outCardGame->largeTextureStreamer = TextureStreamer::Create(outCardGame->arena, 8, 32, imageCreateInfo, LARGE_CARD_ART_SHAPE.x);
		const auto dynBossTexts = GetDynamicBossTexturePaths(outCardGame->engine.GetMemory().arena, outCardGame->engine.GetMemory().frameArena);
		for (const auto& dynText : dynBossTexts)
			outCardGame->largeTextureStreamer.DefineTexturePath(dynText);

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

	void CardGame::Destroy(CardGame& cardGame)
	{
		ma_engine_uninit(&cardGame.audioEngine);
		jv::Arena::Destroy(cardGame.arena);
		Engine::Destroy(cardGame.engine);
	}

	jv::Array<const char*> CardGame::GetTexturePaths(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<const char*>(arena, static_cast<uint32_t>(TextureId::length));
		arr[0] = "Art/alphabet.png";
		arr[1] = "Art/largealphabet.png";
		arr[2] = "Art/numbers.png";
		arr[3] = "Art/symbols.png";
		arr[4] = "Art/mouse.png";
		arr[5] = "Art/card.png";
		arr[6] = "Art/stats.png";
		arr[7] = "Art/flowers.png";
		arr[8] = "Art/fallback.png";
		arr[9] = "Art/empty.png";
		arr[10] = "Art/manabar.png";
		arr[11] = "Art/manabar_hollow.png";
		arr[12] = "Art/manacrystal.png";
		arr[13] = "Art/moon.png";
		arr[14] = "Art/card_small.png";
		arr[15] = "Art/text_bubble.png";
		arr[16] = "Art/text_bubble_tail.png";
		arr[17] = "Art/large_numbers.png";
		arr[18] = "Art/title.png";
		arr[19] = "Art/pause_menu.png";
		return arr;
	}

	jv::Array<const char*> CardGame::GetDynamicTexturePaths(jv::Arena& arena, jv::Arena& frameArena)
	{
		const auto arr = jv::CreateArray<const char*>(frameArena, ALL_ID_COUNT);
		for (uint32_t i = 0; i < arr.length; ++i)
		{
			/*
			const char* prefix = "Art/Monsters/";
			arr[i] = arr[i + l] = TextInterpreter::Concat(prefix, TextInterpreter::IntToConstCharPtr(i + 1, frameArena), frameArena);
			arr[i + l] = TextInterpreter::Concat(arr[i], "_norm.png", arena);
			arr[i] = TextInterpreter::Concat(arr[i], ".png", arena);
			*/

			arr[i] = "Art/Artifact/the_brand.png";
		}
		
		arr[MONSTER_IDS::DAISY] = "Art/Monsters/daisy.png";
		arr[MONSTER_IDS::GOBLIN] = "Art/Monsters/goblin.png";
		arr[MONSTER_IDS::DEMON] = "Art/Monsters/demon.png";
		arr[MONSTER_IDS::LIBRARIAN] = "Art/Monsters/librarian.png";
		arr[MONSTER_IDS::MAD_PYROMANCER] = "Art/Monsters/mad_pyromancer.png";
		arr[MONSTER_IDS::CHAOS_CLOWN] = "Art/Monsters/chaos_clown.png";
		arr[MONSTER_IDS::PHANTASM] = "Art/Monsters/phantasm.png";
		arr[MONSTER_IDS::MANA_DEVOURER] = "Art/Monsters/mana_devourer.png";
		arr[MONSTER_IDS::SLIME] = "Art/Monsters/slime.png";
		arr[MONSTER_IDS::SLIME_SOLDIER] = "Art/Monsters/slime_soldier.png";
		arr[MONSTER_IDS::GOBLIN_CHAMPION] = "Art/Monsters/goblin_champion.png";
		arr[MONSTER_IDS::GOBLIN_KING] = "Art/Monsters/goblin_king.png";
		arr[MONSTER_IDS::GOBLIN_BOMBER] = "Art/Monsters/goblin_bomber.png";
		arr[MONSTER_IDS::KNIFE_JUGGLER] = "Art/Monsters/knife_juggler.png";
		arr[MONSTER_IDS::ELVEN_SAGE] = "Art/Monsters/elven_sage.png";
		arr[MONSTER_IDS::TREASURE_GOBLIN] = "Art/Monsters/treasure_goblin.png";
		arr[MONSTER_IDS::GOBLIN_PRINCESS] = "Art/Monsters/goblin_princess.png";
		arr[MONSTER_IDS::GNOME_SCOUT] = "Art/Monsters/goblin_scout.png";
		arr[MONSTER_IDS::VULTURE] = "Art/Monsters/vulture.png";
		arr[MONSTER_IDS::STORM_ELEMENTAL] = "Art/Monsters/storm_elemental.png";
		arr[MONSTER_IDS::MOSSY_ELEMENTAL] = "Art/Monsters/mossy_elemental.png";
		arr[MONSTER_IDS::ELF] = "Art/Monsters/elf.png";
		arr[MONSTER_IDS::PESKY_PARASITE] = "Art/Monsters/pesky_parasite.png";
		arr[MONSTER_IDS::UNSTABLE_CREATION] = "Art/Monsters/unstable_creation.png";
		arr[MONSTER_IDS::WOUNDED_PANDANA] = "Art/Monsters/wounded_troll.png";
		arr[MONSTER_IDS::MOON_ACOLYTE] = "Art/Monsters/moon_acolyte.png";
		arr[MONSTER_IDS::BERSERKER] = "Art/Monsters/berserker.png";
		arr[MONSTER_IDS::BEAST_SPIRIT] = "Art/Monsters/beast_spirit.png";
		arr[MONSTER_IDS::DUNG_LOBBER] = "Art/Monsters/dung_lobber.png";
		arr[MONSTER_IDS::SLIME_HEAD] = "Art/Monsters/slime_head.png";
		arr[MONSTER_IDS::FOREST_SPIRIT] = "Art/Monsters/forest_spirit.png";
		
		arr[SPELL_ID_START + SPELL_IDS::ARCANE_INTELLECT] = "Art/Spells/arcane_intellect.png";
		arr[SPELL_ID_START + SPELL_IDS::DREAD_SACRIFICE] = "Art/Spells/dread_sacrifice.png";
		arr[SPELL_ID_START + SPELL_IDS::TRANQUILIZE] = "Art/Spells/tranquilize.png";
		arr[SPELL_ID_START + SPELL_IDS::DOUBLE_TROUBLE] = "Art/Spells/double_trouble.png";
		arr[SPELL_ID_START + SPELL_IDS::GOBLIN_AMBUSH] = "Art/Spells/goblin_ambush.png";
		arr[SPELL_ID_START + SPELL_IDS::WINDFALL] = "Art/Spells/windfall.png";
		arr[SPELL_ID_START + SPELL_IDS::RALLY] = "Art/Spells/rally.png";
		arr[SPELL_ID_START + SPELL_IDS::HOLD_THE_LINE] = "Art/Spells/hold_the_line.png";
		arr[SPELL_ID_START + SPELL_IDS::RAMPANT_GROWTH] = "Art/Spells/rampant_growth.png";
		arr[SPELL_ID_START + SPELL_IDS::ENLIGHTENMENT] = "Art/Spells/enlightenment.png";
		arr[SPELL_ID_START + SPELL_IDS::DRUIDIC_RITUAL] = "Art/Spells/druidic_ritual.png";
		arr[SPELL_ID_START + SPELL_IDS::ASCENSION] = "Art/Spells/ascension.png";
		arr[SPELL_ID_START + SPELL_IDS::STAMPEDE] = "Art/Spells/stampede.png";
		arr[SPELL_ID_START + SPELL_IDS::ENRAGE] = "Art/Spells/enrage.png";
		arr[SPELL_ID_START + SPELL_IDS::INFURIATE] = "Art/Spells/infuriate.png";
		arr[SPELL_ID_START + SPELL_IDS::GOBLIN_CHANT] = "Art/Spells/goblin_chant.png";
		arr[SPELL_ID_START + SPELL_IDS::TREASURE_HUNT] = "Art/Spells/treasure_hunt.png";
		arr[SPELL_ID_START + SPELL_IDS::REWIND] = "Art/Spells/rewind.png";
		arr[SPELL_ID_START + SPELL_IDS::MANA_SURGE] = "Art/Spells/mana_surge.png";
		arr[SPELL_ID_START + SPELL_IDS::DARK_RITUAL] = "Art/Spells/dark_ritual.png";
		arr[SPELL_ID_START + SPELL_IDS::FLAME_BOLT] = "Art/Spells/flame_bolt.png";
		arr[SPELL_ID_START + SPELL_IDS::SHOCK] = "Art/Spells/shock.png";
		arr[SPELL_ID_START + SPELL_IDS::PYROBLAST] = "Art/Spells/pyroblast.png";
		arr[SPELL_ID_START + SPELL_IDS::UNSTABLE_COPY] = "Art/Spells/unstable_copy.png";
		arr[SPELL_ID_START + SPELL_IDS::PERFECT_COPY] = "Art/Spells/perfect_copy.png";
		arr[SPELL_ID_START + SPELL_IDS::PAIN_FOR_GAIN] = "Art/Spells/pain_for_gain.png";
		arr[SPELL_ID_START + SPELL_IDS::BETRAYAL] = "Art/Spells/betrayal.png";
		arr[SPELL_ID_START + SPELL_IDS::INCANTATION_OF_DOOM] = "Art/Spells/incantation_of_doom.png";
		arr[SPELL_ID_START + SPELL_IDS::PROTECT] = "Art/Spells/protect.png";
		arr[SPELL_ID_START + SPELL_IDS::SHIELD] = "Art/Spells/shield.png";
		arr[SPELL_ID_START + SPELL_IDS::ENCITEMENT] = "Art/Spells/encitement.png";
		arr[SPELL_ID_START + SPELL_IDS::STALL] = "Art/Spells/stall.png";
		arr[SPELL_ID_START + SPELL_IDS::BALANCE] = "Art/Spells/balance.png";
		arr[SPELL_ID_START + SPELL_IDS::COUNTER_BALANCE] = "Art/Spells/counter_balance.png";
		arr[SPELL_ID_START + SPELL_IDS::GRIT] = "Art/Spells/grit.png";
		arr[SPELL_ID_START + SPELL_IDS::PARIAH] = "Art/Spells/pariah.png";
		arr[SPELL_ID_START + SPELL_IDS::FIRE_STORM] = "Art/Spells/fire_storm.png";
		arr[SPELL_ID_START + SPELL_IDS::UNITY] = "Art/Spells/unity.png";
		arr[SPELL_ID_START + SPELL_IDS::TRIBALISM] = "Art/Spells/tribalism.png";
		arr[SPELL_ID_START + SPELL_IDS::LOYAL_WORKFORCE] = "Art/Spells/loyal_workforce.png";
		arr[SPELL_ID_START + SPELL_IDS::PICK] = "Art/Spells/pick.png";
		arr[SPELL_ID_START + SPELL_IDS::CYCLE] = "Art/Spells/cycle.png";
		
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::HELMET_OF_HATE] = "Art/Artifact/helmet_of_hate.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MASK_OF_THE_EMPEROR] = "Art/Artifact/mask_of_the_emperor.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MASK_OF_CHAOS] = "Art/Artifact/mask_of_chaos.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::THORNMAIL] = "Art/Artifact/thorn_mail.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::BOOTS_OF_SWIFTNESS] = "Art/Artifact/boots_of_swiftness.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::REVERSE_CARD] = "Art/Artifact/reverse_card.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MAGE_HAT] = "Art/Artifact/mage_hat.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::CUP_OF_BLOOD] = "Art/Artifact/cup_of_blood.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::CORRUPTING_KNIFE] = "Art/Artifact/corrupting_knife.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MASK_OF_YOUTH] = "Art/Artifact/mask_of_youth.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MOANING_ORB] = "Art/Artifact/moaning_orb.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::THE_BRAND] = "Art/Artifact/the_brand.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::BLESSED_RING] = "Art/Artifact/blessed_ring.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MANA_RING] = "Art/Artifact/mana_ring.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::BLOOD_AXE] = "Art/Artifact/blood_axe.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::ARCANE_AMULET] = "Art/Artifact/arcane_amulet.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::MOON_SCYTE] = "Art/Artifact/moon_scyte.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::SWORD_OF_SPELLCASTING] = "Art/Artifact/sword_of_spellcasting.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::THORN_WHIP] = "Art/Artifact/thorn_whip.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::STAFF_OF_SUMMONING] = "Art/Artifact/staff_of_summoning.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::JESTER_HAT] = "Art/Artifact/jester_hat.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::QUICK_BOW] = "Art/Artifact/quick_bow.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::FORBIDDEN_TOME] = "Art/Artifact/forbidden_tome.png";
		arr[ARTIFACT_ID_START + ARTIFACT_IDS::CRYSTAL_FLOWER] = "Art/Artifact/crystal_flower.png";

		arr[CURSE_ID_START + CURSE_IDS::FADING] = "Art/Curses/fading.png";
		arr[CURSE_ID_START + CURSE_IDS::WEAKNESS] = "Art/Curses/weakness.png";
		arr[CURSE_ID_START + CURSE_IDS::COWARDICE] = "Art/Curses/cowardice.png";
		arr[CURSE_ID_START + CURSE_IDS::DUM_DUM] = "Art/Curses/dum_dum.png";
		arr[CURSE_ID_START + CURSE_IDS::HATE] = "Art/Curses/hate.png";
		arr[CURSE_ID_START + CURSE_IDS::HAUNTING] = "Art/Curses/haunting.png";
		arr[CURSE_ID_START + CURSE_IDS::TIME] = "Art/Curses/time.png";
		arr[CURSE_ID_START + CURSE_IDS::VULNERABILITY] = "Art/Curses/vulnerability.png";

		arr[EVENT_ID_START + EVENT_IDS::AETHER_SURGE] = "Art/Spells/arcane_intellect.png";
		arr[EVENT_ID_START + EVENT_IDS::DOPPLEGANGSTERS] = "Art/Spells/unstable_copy.png";
		arr[EVENT_ID_START + EVENT_IDS::GATHERING_STORM] = "Art/Spells/counter_balance.png";
		arr[EVENT_ID_START + EVENT_IDS::RETRIBUTION] = "Art/Spells/hold_the_line.png";
		arr[EVENT_ID_START + EVENT_IDS::GOBLIN_PLAGUE] = "Art/Spells/goblin_ambush.png";
		arr[EVENT_ID_START + EVENT_IDS::WHIRLWIND] = "Art/Spells/windfall.png";
		arr[EVENT_ID_START + EVENT_IDS::HEALING_WORD] = "Art/Spells/protect.png";
		arr[EVENT_ID_START + EVENT_IDS::BRIEF_RESPISE] = "Art/Spells/balance.png";
		arr[EVENT_ID_START + EVENT_IDS::CHASE_THE_DRAGON] = "Art/Spells/stampede.png";

		arr[ROOM_ID_START + ROOM_IDS::FIELD_OF_VENGEANCE] = "Art/Spells/goblin_chant.png";
		arr[ROOM_ID_START + ROOM_IDS::FORSAKEN_BATTLEFIELD] = "Art/Spells/dread_sacrifice.png";
		arr[ROOM_ID_START + ROOM_IDS::BLESSED_HALLS] = "Art/Spells/perfect_copy.png";
		arr[ROOM_ID_START + ROOM_IDS::DOMAIN_OF_PAIN] = "Art/Spells/incantation_of_doom.png";
		arr[ROOM_ID_START + ROOM_IDS::ARENA_OF_THE_DAMNED] = "Art/Spells/rally.png";
		arr[ROOM_ID_START + ROOM_IDS::TRANQUIL_WATERS] = "Art/Spells/pick.png";
		arr[ROOM_ID_START + ROOM_IDS::PLAIN_MEADOWS] = "Art/Spells/rampant_growth.png";
		arr[ROOM_ID_START + ROOM_IDS::PRISON_OF_ETERNITY] = "Art/Spells/stall.png";
		return arr;
	}

	jv::Array<const char*> CardGame::GetDynamicBossTexturePaths(jv::Arena& arena, jv::Arena& frameArena)
	{
		const auto arr = jv::CreateArray<const char*>(frameArena, 3 * TOTAL_BOSS_COUNT);
		for (uint32_t i = 0; i < arr.length; ++i)
		{
			/*
			const char* prefix = "Art/Monsters/B";
			arr[i] = arr[i + l] = TextInterpreter::Concat(prefix, TextInterpreter::IntToConstCharPtr(i + 1, frameArena), frameArena);
			arr[i + l] = TextInterpreter::Concat(arr[i], "_norm.png", arena);
			arr[i] = TextInterpreter::Concat(arr[i], ".png", arena);
			*/
			arr[i] = "Art/Monsters/the_pontiff.png";
		}

		arr[MONSTER_IDS::DARK_CRESCENT - BOSS_ID_INDEX_SUB] = "Art/Monsters/dark_crescent.png";
		arr[MONSTER_IDS::GHOSTFLAME_PONTIFF - BOSS_ID_INDEX_SUB] = "Art/Monsters/the_pontiff.png";
		//arr[0] = "Art/Monsters/Slime_King.png";

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
		{
			//card.normalAnimIndex = c;
			card.animIndex = c++;
		}

		auto& vulture = arr[MONSTER_IDS::VULTURE];
		vulture.name = "vulture";
		vulture.health = 1;
		vulture.attack = 1;
		vulture.tags = TAG_TOKEN | TAG_BEAST;
		vulture.unique = true;
		vulture.onCastText = "keh...";
		vulture.onBuffedText = "khe khe khe";
		vulture.onDamagedText = "kaaaaah";
		vulture.onAttackText = "kah";
		vulture.onAllySummonText = "ki ki ki";
		vulture.onEnemySummonText = "khuuh";
		vulture.onEnemyDeathText = "kyheheheh";
		vulture.onAllyDeathText = "kyhahahah";
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
		goblin.onCastText = "magic scary";
		goblin.onBuffedText = "me strong now";
		goblin.onDamagedText = "run";
		goblin.onAttackText = "stabbidy stab";
		goblin.onAllySummonText = "together goblin strong";
		goblin.onEnemySummonText = "they ambush";
		goblin.onAllyDeathText = "hide";
		goblin.onEnemyDeathText = "more meat";
		auto& demon = arr[MONSTER_IDS::DEMON];
		demon.name = "demon";
		demon.health = 0;
		demon.attack = 0;
		demon.tags = TAG_TOKEN;
		demon.unique = true;
		demon.onCastText = "this dark magic...";
		demon.onBuffedText = "yes....yeees";
		demon.onDamagedText = "you dare stand against me";
		demon.onAttackText = "i am unbound";
		demon.onAllySummonText = "more souls to harvest";
		demon.onEnemySummonText = "they will know fear";
		demon.onAllyDeathText = "a pity";
		demon.onEnemyDeathText = "a new soul to torment";
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
		elf.onCastText = "ooh...sparkles";
		elf.onBuffedText = "that tickles";
		elf.onDamagedText = "thats not fun";
		elf.onAttackText = "adult attack";
		elf.onAllySummonText = "more friends to play with";
		elf.onEnemySummonText = "they dont look friendly";
		elf.onAllyDeathText = "i will miss you";
		elf.onEnemyDeathText = "they got tired of playing";
		auto& treasureGoblin = arr[MONSTER_IDS::TREASURE_GOBLIN];
		treasureGoblin.name = "treasure goblin";
		treasureGoblin.health = 1;
		treasureGoblin.attack = 0;
		treasureGoblin.tags = TAG_TOKEN | TAG_GOBLIN;
		treasureGoblin.unique = true;
		treasureGoblin.ruleText = "[death] +1 mana.";
		treasureGoblin.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		elf.onCastText = "spell shiny...like loot";
		elf.onBuffedText = "loot shine harder now";
		elf.onDamagedText = "my gold...my gold";
		elf.onAttackText = "give me loot";
		elf.onAllySummonText = "i no share gold";
		elf.onEnemySummonText = "you no take loot";
		elf.onAllyDeathText = "all gold mine now";
		elf.onEnemyDeathText = "thieves...thieves";
		auto& slime = arr[MONSTER_IDS::SLIME];
		slime.name = "slime";
		slime.attack = 0;
		slime.health = 0;
		slime.unique = true;
		slime.tags = TAG_TOKEN | TAG_SLIME;
		slime.onCastText = "ppphhhzz";
		slime.onBuffedText = "prhrh";
		slime.onDamagedText = "phzzzhzhzhhz";
		slime.onAttackText = "pzz";
		slime.onAllySummonText = "phrzhzhz";
		slime.onEnemySummonText = "hzhrhzh";
		slime.onAllyDeathText = "hzr";
		slime.onEnemyDeathText = "rrrrrhz";
		auto& daisy = arr[MONSTER_IDS::DAISY];
		daisy.name = "daisy";
		daisy.ruleText = "[summon] heal to full.";
		daisy.attack = DAISY_MOD_STATS;
		daisy.health = DAISY_MOD_STATS;
		daisy.unique = true;
		daisy.normalAnimIndex = daisy.animIndex;
		daisy.tags = TAG_BEAST;
		daisy.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onSummon)
				{
					if (actionState.dst != self)
						return false;
					const auto& boardState = state.boardState;
					
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatSet;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.srcUniqueId = state.boardState.uniqueIds[self];
					buffState.dst = self;
					buffState.dstUniqueId = buffState.srcUniqueId;
					buffState.values[ActionState::VStatSet::health] = DAISY_MOD_STATS;
					state.TryAddToStack(buffState);

					return true;
				}
				return false;
			};
		daisy.onCastText = "lets go";
		daisy.onBuffedText = "huuugh";
		daisy.onDamagedText = "oww";
		daisy.onAttackText = "haa";
		daisy.onAllySummonText = "good";
		daisy.onEnemySummonText = "hmm";
		daisy.onAllyDeathText = "noooooooooooooooooo oooooooooooooooooo oooooooooooooooooo";
		daisy.onEnemyDeathText = "lets go";
		
		auto& darkCrescent = arr[MONSTER_IDS::DARK_CRESCENT];
		darkCrescent.name = "dark crescent";
		darkCrescent.attack = 0;
		darkCrescent.health = 999;
		darkCrescent.unique = true;
		darkCrescent.ruleText = "[attacked, cast] deal 1 damage to all enemies and gain 1 attack.";
		darkCrescent.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast || actionState.trigger == ActionState::Trigger::onAttack)
				{
					if (actionState.trigger == ActionState::Trigger::onAttack && actionState.dst != self)
						return false;

					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::board;
					buffState.src = self;
					buffState.dst = self;
					buffState.srcUniqueId = state.boardState.uniqueIds[self];
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
		darkCrescent.tags = TAG_BOSS;
		darkCrescent.animIndex = 0;

		auto& greatTroll = arr[MONSTER_IDS::GREAT_TROLL];
		greatTroll.name = "great troll";
		greatTroll.attack = 5;
		greatTroll.health = 75;
		greatTroll.ruleText = "[attacked, cast] +1 bonus attack.";
		greatTroll.unique = true;
		greatTroll.tags = TAG_BOSS;
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
		greatTroll.animIndex = 1;

		auto& slimeQueen = arr[MONSTER_IDS::SLIME_QUEEN];
		slimeQueen.name = "slime queen";
		slimeQueen.attack = 2;
		slimeQueen.health = 35;
		slimeQueen.ruleText = "[end of turn] summon a slime soldier with my stats.";
		slimeQueen.unique = true;
		slimeQueen.tags = TAG_SLIME | TAG_BOSS;
		slimeQueen.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					const auto& boardState = state.boardState;
					const auto& stats = boardState.combatStats[self];
					
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::SLIME_SOLDIER;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					summonState.values[ActionState::VSummon::health] = stats.health + stats.tempHealth;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		slimeQueen.animIndex = 2;

		auto& ghostflamePontiff = arr[MONSTER_IDS::GHOSTFLAME_PONTIFF];
		ghostflamePontiff.name = "ghostflame pontiff";
		ghostflamePontiff.attack = 0;
		ghostflamePontiff.health = 100;
		ghostflamePontiff.ruleText = "[start of turn] +1 attack and +3 bonus health.";
		ghostflamePontiff.unique = true;
		ghostflamePontiff.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		ghostflamePontiff.tags = TAG_BOSS;
		ghostflamePontiff.animIndex = 0;
		ghostflamePontiff.onCastText = "your magic...fails you";
		ghostflamePontiff.onBuffedText = "this power...it flows through me";
		ghostflamePontiff.onDamagedText = "there is no...hope";
		ghostflamePontiff.onAttackText = "by ghostflame be...purged";
		ghostflamePontiff.onAllySummonText = "to me...minions";
		ghostflamePontiff.onEnemySummonText = "abandon...hope";
		ghostflamePontiff.onAllyDeathText = "ghostflame consume thee";
		ghostflamePontiff.onEnemyDeathText = "one less...";
		ghostflamePontiff.animIndex = 3;

		auto& mirrorKnight = arr[MONSTER_IDS::MIRROR_KNIGHT];
		mirrorKnight.name = "mirror knight";
		mirrorKnight.attack = 6;
		mirrorKnight.health = 100;
		mirrorKnight.ruleText = "[start of turn] summon 1/1 copies of each enemy monster.";
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
						summonState.values[ActionState::VSummon::attack] = 1;
						summonState.values[ActionState::VSummon::health] = 1;
						state.TryAddToStack(summonState);
					}
					
					return true;
				}
				return false;
			};
		mirrorKnight.tags = TAG_BOSS;
		mirrorKnight.animIndex = 4;

		auto& bomba = arr[MONSTER_IDS::BOMBA];
		bomba.name = "bomba";
		bomba.attack = 10;
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
		bomba.tags = TAG_BOSS;
		bomba.animIndex = 5;

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
		theDread.attack = 5;
		theDread.health = 555;
		theDread.ruleText = "[end of turn] summon the dread. [start of turn 5] dies.";
		theDread.unique = true;
		theDread.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					if(state.turn == 5)
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
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState summonState{};
					summonState.trigger = ActionState::Trigger::onSummon;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::THE_DREAD;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		theDread.tags = TAG_BOSS;
		theDread.animIndex = 6;

		auto& archmage = arr[MONSTER_IDS::ARCHMAGE];
		archmage.name = "archmage";
		archmage.attack = 8;
		archmage.health = 200;
		archmage.ruleText = "[enemy buff] copy it for myself. [end of turn] deal 2 damage to all enemies.";
		archmage.unique = true;
		archmage.tags = TAG_HUMAN | TAG_BOSS;
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
					state.TryAddToStack(cpy);
					return true;
				}
				return false;
			};
		archmage.animIndex = 7;

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
		shelly.tags = TAG_BOSS;
		shelly.animIndex = 8;

		auto& goblinQueen = arr[MONSTER_IDS::GOBLIN_QUEEN];
		goblinQueen.name = "goblin queen";
		goblinQueen.attack = 0;
		goblinQueen.health = 200;
		goblinQueen.ruleText = "[start of turn] fill the board with goblins. [any death] give all goblins +1 attack.";
		goblinQueen.unique = true;
		goblinQueen.tags = TAG_GOBLIN | TAG_BOSS;
		goblinQueen.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
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
					return true;
				}
				return false;
			};
		goblinQueen.animIndex = 9;
		
		auto& masterLich = arr[MONSTER_IDS::MASTER_LICH];
		masterLich.name = "master lich";
		masterLich.attack = 10;
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
					state.TryAddToStack(buffState);
					return true;
				}
				return false;
			};
		masterLich.tags = TAG_BOSS;
		masterLich.animIndex = 10;

		auto& theCollector = arr[MONSTER_IDS::THE_COLLECTOR];
		theCollector.name = "the collector";
		theCollector.attack = 15;
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
		theCollector.tags = TAG_BOSS;
		theCollector.animIndex = 11;

		auto& slimeEmperor = arr[MONSTER_IDS::SLIME_EMPEROR];
		slimeEmperor.name = "slime emperor";
		slimeEmperor.attack = 10;
		slimeEmperor.health = 300;
		slimeEmperor.ruleText = "[damaged] summon a slime with attack and health equal to the damage taken.";
		slimeEmperor.unique = true;
		slimeEmperor.tags = TAG_SLIME | TAG_BOSS;
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
		slimeEmperor.animIndex = 12;

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
		knifeJuggler.onCastText = "draw cards already";
		knifeJuggler.onBuffedText = "not my speciality but sure";
		knifeJuggler.onDamagedText = "hey...protect me";
		knifeJuggler.onAttackText = "now...stand still";
		knifeJuggler.onAllySummonText = "wanna see my knifes";
		knifeJuggler.onEnemySummonText = "more target practice";
		knifeJuggler.onAllyDeathText = "knifes out";
		knifeJuggler.onEnemyDeathText = "headshot";

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
		elvenSage.onCastText = "nature grows";
		elvenSage.onBuffedText = "nature heals";
		elvenSage.onDamagedText = "do not defy nature";
		elvenSage.onAttackText = "i will guide you";
		elvenSage.onAllySummonText = "serve the forest";
		elvenSage.onEnemySummonText = "a threat to the foreset";
		elvenSage.onAllyDeathText = "and now return to the cycle";
		elvenSage.onEnemyDeathText = "nature is healing";

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
		stormElemental.onCastText = "a wind blows";
		stormElemental.onBuffedText = "a storm gathers";
		stormElemental.onDamagedText = "there is a tempest in me";
		stormElemental.onAttackText = "feel the power of a thousand storms";
		stormElemental.onAllySummonText = "a new wind blows";
		stormElemental.onEnemySummonText = "you will be swept away";
		stormElemental.onAllyDeathText = "we will weather this loss";
		stormElemental.onEnemyDeathText = "be swept away";

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
		mossyElemental.onCastText = "...magic...";
		mossyElemental.onBuffedText = "...stronger...";
		mossyElemental.onDamagedText = "...pointless...";
		mossyElemental.onAttackText = "...deliver...";
		mossyElemental.onAllySummonText = "...friend...";
		mossyElemental.onEnemySummonText = "...threat...";
		mossyElemental.onAllyDeathText = "...sleep...";
		mossyElemental.onEnemyDeathText = "...begone...";

		auto& forestSpirit = arr[MONSTER_IDS::FOREST_SPIRIT];
		forestSpirit.name = "forest spirit";
		forestSpirit.attack = 1;
		forestSpirit.health = 20;
		forestSpirit.ruleText = "[cast] +2 bonus attack and health.";
		forestSpirit.tags = TAG_ELEMENTAL;
		forestSpirit.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		forestSpirit.onCastText = "a magical song";
		forestSpirit.onBuffedText = "i can feel that";
		forestSpirit.onDamagedText = "ow...you stepped on my toes";
		forestSpirit.onAttackText = "dance off";
		forestSpirit.onAllySummonText = "dance with me";
		forestSpirit.onEnemySummonText = "another one";
		forestSpirit.onAllyDeathText = "no more dancing for you";
		forestSpirit.onEnemyDeathText = "you have danced your last...dance";

		auto& goblinKing = arr[MONSTER_IDS::GOBLIN_KING];
		goblinKing.name = "goblin king";
		goblinKing.attack = 1;
		goblinKing.health = 16;
		goblinKing.ruleText = "[start of turn] summon a goblin.";
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
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		goblinKing.onCastText = "magic is beneath me";
		goblinKing.onBuffedText = "bow before me";
		goblinKing.onDamagedText = "you dare strike a king";
		goblinKing.onAttackText = "kneel before your king";
		goblinKing.onAllySummonText = "now serve";
		goblinKing.onEnemySummonText = "now bow";
		goblinKing.onAllyDeathText = "a sacrifice i was willing to make";
		goblinKing.onEnemyDeathText = "fall before me";

		auto& goblinChampion = arr[MONSTER_IDS::GOBLIN_CHAMPION];
		goblinChampion.name = "goblin champion";
		goblinChampion.attack = 1;
		goblinChampion.health = 20;
		goblinChampion.ruleText = "[buffed] all other goblins gain +2 bonus attack.";
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
					buffState.values[ActionState::VStatBuff::tempAttack] = 2;
					TargetOfType(info, state, buffState, self, TAG_GOBLIN, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		goblinChampion.onCastText = "magic sucks";
		goblinChampion.onBuffedText = "good..make me stronger";
		goblinChampion.onDamagedText = "you think this hurts";
		goblinChampion.onAttackText = "be crushed";
		goblinChampion.onAllySummonText = "more fodder";
		goblinChampion.onEnemySummonText = "i am the strongest";
		goblinChampion.onAllyDeathText = "weakling";
		goblinChampion.onEnemyDeathText = "for my king";

		auto& unstableCreation = arr[MONSTER_IDS::UNSTABLE_CREATION];
		unstableCreation.name = "unstable creation";
		unstableCreation.attack = 4;
		unstableCreation.health = 16;
		unstableCreation.tags = TAG_TOKEN | TAG_ELEMENTAL;
		unstableCreation.onCastText = "aaah";
		unstableCreation.onBuffedText = "uuuuugh";
		unstableCreation.onDamagedText = "uuuauauah";
		unstableCreation.onAttackText = "uuughh....";
		unstableCreation.onAllySummonText = "hhhhh....";
		unstableCreation.onEnemySummonText = "hhhhhh....";
		unstableCreation.onAllyDeathText = "uuuahh";
		unstableCreation.onEnemyDeathText = "uuh...uuuuh";

		auto& moonAcolyte = arr[MONSTER_IDS::MOON_ACOLYTE];
		moonAcolyte.name = "moon acolyte";
		moonAcolyte.attack = 1;
		moonAcolyte.health = 20;
		moonAcolyte.ruleText = "[any death] +2 attack and +2 bonus attack.";
		moonAcolyte.tags = TAG_BEAST;
		moonAcolyte.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		moonAcolyte.onCastText = "moon magic guide you";
		moonAcolyte.onBuffedText = "the moon speaks";
		moonAcolyte.onDamagedText = "moon maiden protect me";
		moonAcolyte.onAttackText = "moon goddess smite thee";
		moonAcolyte.onAllySummonText = "serve the moon maiden";
		moonAcolyte.onEnemySummonText = "heretic";
		moonAcolyte.onAllyDeathText = "you will serve me in death";
		moonAcolyte.onEnemyDeathText = "a sacrifice to the great wheel of cheese";

		auto& berserker = arr[MONSTER_IDS::BERSERKER];
		berserker.name = "berserker";
		berserker.attack = 1;
		berserker.health = 20;
		berserker.ruleText = "[damaged] attack the lowest health enemy.";
		berserker.tags = TAG_BEAST;
		berserker.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		berserker.onCastText = "magic is for the weak";
		berserker.onBuffedText = "blood...give me blood";
		berserker.onDamagedText = "more more more more more more";
		berserker.onAttackText = "die die die die die die die die";
		berserker.onAllySummonText = "step aside";
		berserker.onEnemySummonText = "mother...i crave violence";
		berserker.onAllyDeathText = "weakness disgusts me";
		berserker.onEnemyDeathText = "more skulls for my throne";

		auto& manaDevourer = arr[MONSTER_IDS::MANA_DEVOURER];
		manaDevourer.name = "mana devourer";
		manaDevourer.attack = 1;
		manaDevourer.health = 20;
		manaDevourer.ruleText = "[end of turn] gain 2 attack per unspent mana.";
		manaDevourer.tags = TAG_ELEMENTAL;
		manaDevourer.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		manaDevourer.onCastText = "ma..giiic...";
		manaDevourer.onBuffedText = "mmm...ooore";
		manaDevourer.onDamagedText = "mhhaaaaa...";
		manaDevourer.onAttackText = "mmmiiiineee..";
		manaDevourer.onAllySummonText = "mmmmmaaa...nnna";
		manaDevourer.onEnemySummonText = "mmman...aaa";
		manaDevourer.onAllyDeathText = "mmmm....";
		manaDevourer.onEnemyDeathText = "aaaan...naaa...";

		auto& woundedTroll = arr[MONSTER_IDS::WOUNDED_PANDANA];
		woundedTroll.name = "wounded pandana";
		woundedTroll.attack = 3;
		woundedTroll.health = 20;
		woundedTroll.ruleText = "[end of turn] take 1 damage.";
		woundedTroll.tags = TAG_BEAST;
		woundedTroll.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onEndOfTurn)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					damageState.dst = self;
					damageState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(damageState);
				}
				return false;
			};
		woundedTroll.onCastText = "thats a healing spell right";
		woundedTroll.onBuffedText = "im still bleeding";
		woundedTroll.onDamagedText = "owww... right on the sore spot";
		woundedTroll.onAttackText = "im bleeding";
		woundedTroll.onAllySummonText = "do you happen to be a doctor";
		woundedTroll.onEnemySummonText = "and im still wounded from the last fight";
		woundedTroll.onAllyDeathText = "oh no... im next";
		woundedTroll.onEnemyDeathText = "i really need to get this looked at";

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
		chaosClown.onCastText = "im playing both sides so that i always come out on top";
		chaosClown.onBuffedText = "you know im gonna die anyway right";
		chaosClown.onDamagedText = "my nose...my nose";
		chaosClown.onAttackText = "you get what you fluckin deserve";
		chaosClown.onAllySummonText = "an ally...or an enemy";
		chaosClown.onEnemySummonText = "one more to clown on";
		chaosClown.onAllyDeathText = "sad clown noises";
		chaosClown.onEnemyDeathText = "guess we were both clowns";

		auto& dungLobber = arr[MONSTER_IDS::DUNG_LOBBER];
		dungLobber.name = "dung lobber";
		dungLobber.attack = 1;
		dungLobber.health = 20;
		dungLobber.ruleText = "[ally attack] the attacked monster takes damage equal to my bonus attack.";
		dungLobber.tags = TAG_BEAST;
		dungLobber.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		dungLobber.onCastText = "confused monkey stare";
		dungLobber.onBuffedText = "happy monkey noises";
		dungLobber.onDamagedText = "screaching noises";
		dungLobber.onAttackText = "poop throwing noises";
		dungLobber.onAllySummonText = "waves in monkey";
		dungLobber.onEnemySummonText = "yells in monkey";
		dungLobber.onAllyDeathText = "sad monkey noises";
		dungLobber.onEnemyDeathText = "angry monkey sounds";

		auto& goblinBomber = arr[MONSTER_IDS::GOBLIN_BOMBER];
		goblinBomber.name = "goblin bomber";
		goblinBomber.attack = 1;
		goblinBomber.health = 20;
		goblinBomber.ruleText = "[any attack] the attacker takes 1 damage.";
		goblinBomber.tags = TAG_GOBLIN;
		goblinBomber.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if(actionState.trigger == ActionState::Trigger::onAttack)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.src;
					damageState.dstUniqueId = actionState.srcUniqueId;
					damageState.values[ActionState::VDamage::damage] = 1;
					state.TryAddToStack(damageState);
					return true;
				}
				
				return false;
			};
		goblinBomber.onCastText = "and boom it goes";
		goblinBomber.onBuffedText = "that feels explosive";
		goblinBomber.onDamagedText = "like a bomb just went off";
		goblinBomber.onAttackText = "catch this";
		goblinBomber.onAllySummonText = "get ready to be blown away";
		goblinBomber.onEnemySummonText = "catch";
		goblinBomber.onAllyDeathText = "what an explosive end";
		goblinBomber.onEnemyDeathText = "explode";

		auto& goblinPrincess = arr[MONSTER_IDS::GOBLIN_PRINCESS];
		goblinPrincess.name = "goblin princess";
		goblinPrincess.attack = 1;
		goblinPrincess.health = 14;
		goblinPrincess.ruleText = "[damaged] summon a goblin.";
		goblinPrincess.tags = TAG_GOBLIN;
		goblinPrincess.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		goblinPrincess.onCastText = "pretty and i know it";
		goblinPrincess.onBuffedText = "i feel empowered";
		goblinPrincess.onDamagedText = "you wouldnt hit a lady";
		goblinPrincess.onAttackText = "im not just pretty";
		goblinPrincess.onAllySummonText = "another one to swoon over me";
		goblinPrincess.onEnemySummonText = "i need a strong goblin to protect me";
		goblinPrincess.onAllyDeathText = "oh well...anyway";
		goblinPrincess.onEnemyDeathText = "i should have looked away";

		auto& peskyParasite = arr[MONSTER_IDS::PESKY_PARASITE];
		peskyParasite.name = "pesky parasite";
		peskyParasite.attack = 1;
		peskyParasite.health = 2;
		peskyParasite.ruleText = "[death] if there is another allied monster, summon a pesky parasite with my attack.";
		peskyParasite.tags = TAG_BEAST;
		peskyParasite.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::PESKY_PARASITE;
					summonState.values[ActionState::VSummon::isAlly] = self < BOARD_CAPACITY_PER_SIDE;
					summonState.values[ActionState::VSummon::attack] = stats.attack + stats.tempAttack;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};
		peskyParasite.onCastText = "skkk";
		peskyParasite.onBuffedText = "skkrtkt";
		peskyParasite.onDamagedText = "skkrkrkrkrkrk";
		peskyParasite.onAttackText = "skr skrss";
		peskyParasite.onAllySummonText = "srktsk";
		peskyParasite.onEnemySummonText = "ksrkkts";
		peskyParasite.onAllyDeathText = "ksrktks";
		peskyParasite.onEnemyDeathText = "ktskrksts";

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
		slimeSoldier.onCastText = "a spell like that gave me the power to talk once";
		slimeSoldier.onBuffedText = "i see you understand why im strong";
		slimeSoldier.onDamagedText = "you know this will make my clones weaker right";
		slimeSoldier.onAttackText = "for the slime emperor";
		slimeSoldier.onAllySummonText = "the power of one...the power of many";
		slimeSoldier.onEnemySummonText = "i will slime you";
		slimeSoldier.onAllyDeathText = "goodbye...other me";
		slimeSoldier.onEnemyDeathText = "get slimed on";

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
		madPyromancer.onCastText = "burn them all";
		madPyromancer.onBuffedText = "hahahaahaaaa";
		madPyromancer.onDamagedText = "we will all burn";
		madPyromancer.onAttackText = "ahahahahahah";
		madPyromancer.onAllySummonText = "flame take us all";
		madPyromancer.onEnemySummonText = "all will be ashes soon";
		madPyromancer.onAllyDeathText = "now serve as fuel for my flames";
		madPyromancer.onEnemyDeathText = "become cinders";

		auto& phantasm = arr[MONSTER_IDS::PHANTASM];
		phantasm.name = "phantasm";
		phantasm.attack = 3;
		phantasm.health = 1;
		phantasm.ruleText = "[start of turn] +9 bonus health. [attack] bonus health becomes 0.";
		phantasm.tags = TAG_ELEMENTAL;
		phantasm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState setState{};
					setState.trigger = ActionState::Trigger::onStatBuff;
					setState.source = ActionState::Source::other;
					setState.values[ActionState::VStatBuff::tempHealth] = 9;
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
		phantasm.onCastText = "feel the cold of death";
		phantasm.onBuffedText = "i have become immortal";
		phantasm.onDamagedText = "it is futile";
		phantasm.onAttackText = "the dead rise";
		phantasm.onAllySummonText = "follow my light";
		phantasm.onEnemySummonText = "you will soon join me in the afterlife";
		phantasm.onAllyDeathText = "i will guide your spirit";
		phantasm.onEnemyDeathText = "i will guide your spirit the wrong way on purpose";

		auto& gnomeScout = arr[MONSTER_IDS::GNOME_SCOUT];
		gnomeScout.name = "gnome scout";
		gnomeScout.attack = 1;
		gnomeScout.health = 16;
		gnomeScout.ruleText = "[attack] all other beasts gain bonus attack equal to my attack.";
		gnomeScout.tags = TAG_BEAST;
		gnomeScout.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					buffState.values[ActionState::VStatBuff::tempAttack] = stats.attack + stats.tempAttack;
					TargetOfType(info, state, buffState, self, TAG_BEAST, TypeTarget::all, true);
					return true;
				}
				return false;
			};
		gnomeScout.onCastText = "heeehhehehe";
		gnomeScout.onBuffedText = "ahahahah";
		gnomeScout.onDamagedText = "hee hee...";
		gnomeScout.onAttackText = "hahahahahah";
		gnomeScout.onAllySummonText = "hihihi";
		gnomeScout.onEnemySummonText = "heh heh heh";
		gnomeScout.onAllyDeathText = "ha ha ha";
		gnomeScout.onEnemyDeathText = "hahaha haa";

		auto& slimeHead = arr[MONSTER_IDS::SLIME_HEAD];
		slimeHead.name = "slime head";
		slimeHead.attack = 1;
		slimeHead.health = 13;
		slimeHead.ruleText = "[damaged] draw.";
		slimeHead.tags = TAG_SLIME;
		slimeHead.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		slimeHead.onCastText = "what do you think of my head";
		slimeHead.onBuffedText = "my head is growing";
		slimeHead.onDamagedText = "i blocked that with my head";
		slimeHead.onAttackText = "head butt";
		slimeHead.onAllySummonText = "i see you looking at my head";
		slimeHead.onEnemySummonText = "yes...my head is real";
		slimeHead.onAllyDeathText = "my hat off to you";
		slimeHead.onEnemyDeathText = "head shot";

		auto& beastSpirit = arr[MONSTER_IDS::BEAST_SPIRIT];
		beastSpirit.name = "beast spirit";
		beastSpirit.attack = 1;
		beastSpirit.health = 20;
		beastSpirit.ruleText = "[beast attack] draw.";
		beastSpirit.tags = TAG_BEAST | TAG_ELEMENTAL;
		beastSpirit.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onAttack)
				{
					const auto& boardState = state.boardState;
					if (!boardState.Validate(actionState, true, true))
						return false;

					const auto& monster = info.monsters[boardState.ids[actionState.src]];
					if ((monster.tags & TAG_BEAST) == 0)
						return false;

					ActionState drawState{};
					drawState.trigger = ActionState::Trigger::onDraw;
					drawState.source = ActionState::Source::other;
					state.TryAddToStack(drawState);
					return true;
				}
				return false;
			};
		beastSpirit.onCastText = "...";
		beastSpirit.onBuffedText = "...";
		beastSpirit.onDamagedText = "...";
		beastSpirit.onAttackText = "...";
		beastSpirit.onAllySummonText = "...";
		beastSpirit.onEnemySummonText = "...";
		beastSpirit.onAllyDeathText = "...";
		beastSpirit.onEnemyDeathText = "...";

		auto& librarian = arr[MONSTER_IDS::LIBRARIAN];
		librarian.name = "librarian";
		librarian.attack = 1;
		librarian.health = 20;
		librarian.ruleText = "[draw] +1 mana.";
		librarian.tags = TAG_HUMAN;
		librarian.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDraw)
				{
					/*
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.values[ActionState::VDamage::damage] = 1;
					damageState.dst = self;
					damageState.dstUniqueId = state.boardState.uniqueIds[self];
					state.TryAddToStack(damageState);
					*/
					++state.mana;
					return true;
				}
				return false;
			};
		librarian.onCastText = "i have read about this";
		librarian.onBuffedText = "knowledge overflowing";
		librarian.onDamagedText = "insolent fool";
		librarian.onAttackText = "the text has been written";
		librarian.onAllySummonText = "another student";
		librarian.onEnemySummonText = "and so the next chapter begins";
		librarian.onAllyDeathText = "your name has been written in my book";
		librarian.onEnemyDeathText = "wiped from the history books";
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, ARTIFACT_IDS::LENGTH);

		uint32_t c = 0;
		for (auto& card : arr)
			card.animIndex = ARTIFACT_ID_START + c++;

		arr[ARTIFACT_IDS::ARCANE_AMULET].name = "arcane amulet";
		arr[ARTIFACT_IDS::ARCANE_AMULET].ruleText = "[start of turn] +1 mana.";
		arr[ARTIFACT_IDS::ARCANE_AMULET].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
			{
				++state.mana;
				return true;
			}
			return false;
		};
		arr[ARTIFACT_IDS::MASK_OF_THE_EMPEROR].name = "mask of the emperor";
		arr[ARTIFACT_IDS::MASK_OF_THE_EMPEROR].ruleText = "[attack] the attacked monster takes damage equal to my bonus health.";
		arr[ARTIFACT_IDS::MASK_OF_THE_EMPEROR].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					state.TryAddToStack(damageState);
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
		arr[ARTIFACT_IDS::MAGE_HAT].name = "mage hat";
		arr[ARTIFACT_IDS::MAGE_HAT].ruleText = "[cast] +2 bonus health.";
		arr[ARTIFACT_IDS::MAGE_HAT].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::MASK_OF_YOUTH].name = "mask of youth";
		arr[ARTIFACT_IDS::MASK_OF_YOUTH].ruleText = "[bonus attack buffed] gain that much bonus health.";
		arr[ARTIFACT_IDS::MASK_OF_YOUTH].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::THE_BRAND].name = "the brand";
		arr[ARTIFACT_IDS::THE_BRAND].ruleText = "[start of turn] die. all allies gain 6 health.";
		arr[ARTIFACT_IDS::THE_BRAND].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
					buffState.values[ActionState::VStatBuff::health] = 6;
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
		arr[ARTIFACT_IDS::QUICK_BOW].name = "quick bow";
		arr[ARTIFACT_IDS::QUICK_BOW].ruleText = "[any death] draw.";
		arr[ARTIFACT_IDS::QUICK_BOW].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::MOON_SCYTE].name = "moon scyte";
		arr[ARTIFACT_IDS::MOON_SCYTE].ruleText = "[any death] +4 bonus attack.";
		arr[ARTIFACT_IDS::MOON_SCYTE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::JESTER_HAT].name = "jester hat";
		arr[ARTIFACT_IDS::JESTER_HAT].ruleText = "[damaged] untap.";
		arr[ARTIFACT_IDS::JESTER_HAT].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::BOOTS_OF_SWIFTNESS].name = "boots of swiftness";
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
		arr[ARTIFACT_IDS::FORBIDDEN_TOME].name = "forbidden tome";
		arr[ARTIFACT_IDS::FORBIDDEN_TOME].ruleText = "[cast] +2 bonus attack.";
		arr[ARTIFACT_IDS::FORBIDDEN_TOME].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::THORN_WHIP].ruleText = "[attack] deal 1 damage to all enemies.";
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
					TargetOfType(info, state, damageState, self, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::MASK_OF_CHAOS].name = "mask of chaos";
		arr[ARTIFACT_IDS::MASK_OF_CHAOS].ruleText = "[start of turn] gain 3 bonus health for every enemy. [end of turn] they all attack me.";
		arr[ARTIFACT_IDS::MASK_OF_CHAOS].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					const auto& boardState = state.boardState;
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.dst = self;
					buffState.dstUniqueId = boardState.uniqueIds[self];
					buffState.values[ActionState::VStatBuff::tempHealth] = 3 * boardState.enemyCount;
					state.TryAddToStack(buffState);

					return true;
				}
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
						state.TryAddToStack(attackState);
					}
					
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::HELMET_OF_HATE].name = "helmet of hate";
		arr[ARTIFACT_IDS::HELMET_OF_HATE].ruleText = "[attack] all allies gain +1 attack.";
		arr[ARTIFACT_IDS::HELMET_OF_HATE].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::MOANING_ORB].name = "moaning orb";
		arr[ARTIFACT_IDS::MOANING_ORB].ruleText = "[any death] +1 mana.";
		arr[ARTIFACT_IDS::MOANING_ORB].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onDeath)
				{
					++state.mana;
					return true;
				}
				return false;
			};
		arr[ARTIFACT_IDS::CUP_OF_BLOOD].name = "cup of blood";
		arr[ARTIFACT_IDS::CUP_OF_BLOOD].ruleText = "[on bonus attack buffed] gain that much attack.";
		arr[ARTIFACT_IDS::CUP_OF_BLOOD].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[ARTIFACT_IDS::CRYSTAL_FLOWER].name = "crystal flower";
		arr[ARTIFACT_IDS::CRYSTAL_FLOWER].ruleText = "[2 or more non combat damage to any target] +1 mana.";
		arr[ARTIFACT_IDS::CRYSTAL_FLOWER].onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arr[2] = MONSTER_IDS::GHOSTFLAME_PONTIFF;
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
			card.animIndex = ROOM_ID_START + c++;

		auto& fieldOfVengeance = arr[ROOM_IDS::FIELD_OF_VENGEANCE];
		fieldOfVengeance.name = "field of violence";
		fieldOfVengeance.ruleText = "[monster is dealt non combat damage] gain 2 attack.";
		fieldOfVengeance.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
		{
			if (actionState.trigger == ActionState::Trigger::onDamage)
			{
				if (actionState.source == ActionState::Source::board)
					return false;

				const auto& boardState = state.boardState;
				
				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::attack] = 2;
				buffState.dst = self;
				buffState.dstUniqueId = boardState.uniqueIds[self];
				state.TryAddToStack(buffState);
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
		blessedHalls.ruleText = "[start of turn] all enemies gain +2 bonus health.";
		blessedHalls.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
				{
					ActionState buffState{};
					buffState.trigger = ActionState::Trigger::onStatBuff;
					buffState.source = ActionState::Source::other;
					buffState.values[ActionState::VStatBuff::tempHealth] = 2;

					TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
					return true;
				}
				return false;
			};
		auto& domainOfPain = arr[ROOM_IDS::DOMAIN_OF_PAIN];
		domainOfPain.name = "domain of pain";
		domainOfPain.ruleText = "[end of turn] all monsters take 1 damage.";
		domainOfPain.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
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
		arenaOfTheDamned.ruleText = "[end of turn] all monsters with the lowest health die.";
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
							state.TryAddToStack(killState);
						}
					}

					for (uint32_t i = 0; i < boardState.enemyCount; ++i)
					{
						const auto health = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i].health;
						if (health == lowestHealth)
						{
							killState.dst = BOARD_CAPACITY_PER_SIDE + i;
							killState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
							state.TryAddToStack(killState);
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
			card.animIndex = SPELL_ID_START + c++;

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
		auto& groupHug = arr[SPELL_IDS::ENCITEMENT];
		groupHug.name = "encitement";
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
		treasureHunt.ruleText = "summon 3 treasure goblins for your opponent.";
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
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::TREASURE_GOBLIN;
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
		auto& pyroblast = arr[SPELL_IDS::PYROBLAST];
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
		betrayal.ruleText = "deal 3 damage 3 times.";
		betrayal.cost = 3;
		betrayal.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onCast && self == actionState.src)
				{
					ActionState damageState{};
					damageState.trigger = ActionState::Trigger::onDamage;
					damageState.source = ActionState::Source::other;
					damageState.dst = actionState.dst;
					damageState.dstUniqueId = actionState.dstUniqueId;
					damageState.values[ActionState::VDamage::damage] = 3;
					for (uint32_t i = 0; i < 3; ++i)
						state.TryAddToStack(damageState);
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
			card.animIndex = CURSE_ID_START + c++;

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
			card.animIndex = EVENT_ID_START + c++;

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
		doppleGangsters.ruleText = "[summon ally] summon a demon for your opponent with the same stats.";
		doppleGangsters.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger == ActionState::Trigger::onSummon)
				{
					if (!actionState.values[ActionState::VSummon::isAlly])
						return false;

					const auto& card = info.monsters[actionState.values[ActionState::VSummon::id]];

					ActionState summonState = actionState;
					summonState.source = ActionState::Source::other;
					summonState.values[ActionState::VSummon::isAlly] = 0;
					summonState.values[ActionState::VSummon::id] = MONSTER_IDS::DEMON;
					summonState.values[ActionState::VSummon::attack] = card.attack;
					summonState.values[ActionState::VSummon::health] = card.health;
					state.TryAddToStack(summonState);
					return true;
				}
				return false;
			};

		auto& gatheringStorm = arr[EVENT_IDS::GATHERING_STORM];
		gatheringStorm.name = "gathering storm";
		gatheringStorm.ruleText = "[start of turn] all enemies gain +1 attack.";
		gatheringStorm.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::attack] = 1;
				TargetOfType(info, state, buffState, 0, -1, TypeTarget::enemies);
				return true;
			};

		auto& noYou = arr[EVENT_IDS::RETRIBUTION];
		noYou.name = "retribution";
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
		goblinPlague.ruleText = "[start of turn] summon 2 goblins.";
		goblinPlague.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.source = ActionState::Source::other;
				summonState.values[ActionState::VSummon::isAlly] = 0;
				summonState.values[ActionState::VSummon::id] = MONSTER_IDS::GOBLIN;
				for (uint32_t i = 0; i < 2; ++i)
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
		healingWord.ruleText = "[start of turn] all enemies gain +2 bonus health.";
		healingWord.onActionEvent = [](const LevelInfo& info, State& state, const ActionState& actionState, const uint32_t self)
			{
				if (actionState.trigger != ActionState::Trigger::onStartOfTurn)
					return false;

				ActionState buffState{};
				buffState.trigger = ActionState::Trigger::onStatBuff;
				buffState.source = ActionState::Source::other;
				buffState.values[ActionState::VStatBuff::tempHealth] = 2;
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
