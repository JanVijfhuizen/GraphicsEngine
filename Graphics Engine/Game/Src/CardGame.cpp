﻿#include "pch_game.h"
#include "CardGame.h"
#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>
#include "Cards/ArtifactCard.h"
#include "Cards/BossCard.h"
#include "Cards/MagicCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/RoomCard.h"
#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/PixelPerfectRenderInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "Levels/Level.h"
#include "Levels/MainLevel.h"
#include "Levels/MainMenuLevel.h"
#include "Levels/NewGameLevel.h"
#include "Levels/PartySelectLevel.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

	struct KeyCallback final
	{
		uint32_t key;
		uint32_t action;
	};

	struct CardGame final
	{
		Engine engine;
		jv::Arena arena;
		jv::Arena levelArena;
		jv::ge::Resource scene;
		jv::ge::Resource levelScene;
		jv::ge::Resource atlas;
		InputState inputState{};
		jv::Array<jv::ge::AtlasTexture> atlasTextures;
		jv::Array<glm::ivec2> subTextureResolutions;
		TaskSystem<RenderTask>* renderTasks;
		TaskSystem<DynamicRenderTask>* dynamicRenderTasks;
		TaskSystem<RenderTask>* priorityRenderTasks;
		TaskSystem<TextTask>* textTasks;
		TaskSystem<TextTask>* priorityTextTasks;
		TaskSystem<PixelPerfectRenderTask>* pixelPerfectRenderTasks;
		InstancedRenderInterpreter<RenderTask>* renderInterpreter;
		InstancedRenderInterpreter<RenderTask>* priorityRenderInterpreter;
		DynamicRenderInterpreter* dynamicRenderInterpreter;
		TextInterpreter* textInterpreter;
		TextInterpreter* priorityTextInterpreter;
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
		jv::Array<BossCard> bosses;
		jv::Array<RoomCard> rooms;
		jv::Array<MagicCard> magic;
		jv::Array<FlawCard> flaws;
		jv::Array<EventCard> events;

		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(const CardGame& cardGame);

		[[nodiscard]] static jv::Array<const char*> GetTexturePaths(jv::Arena& arena);
		[[nodiscard]] static jv::Array<MonsterCard> GetMonsterCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<ArtifactCard> GetArtifactCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<BossCard> GetBossCards(jv::Arena& arena);
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
			{800, 600}, // temp.
			inputState,
			*renderTasks,
			*dynamicRenderTasks,
			*priorityRenderTasks,
			*textTasks,
			*priorityTextTasks,
			*pixelPerfectRenderTasks
		};

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

		result = engine.Update();
		return result;
	}

	void CardGame::Create(CardGame* outCardGame)
	{
		*outCardGame = {};
		{
			EngineCreateInfo engineCreateInfo{};
			engineCreateInfo.onKeyCallback = OnKeyCallback;
			engineCreateInfo.onMouseCallback = OnMouseCallback;
			engineCreateInfo.onScrollCallback = OnScrollCallback;
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
			outCardGame->textTasks = &outCardGame->engine.AddTaskSystem<TextTask>();
			outCardGame->textTasks->Allocate(outCardGame->arena, 32);
			outCardGame->priorityTextTasks = &outCardGame->engine.AddTaskSystem<TextTask>();
			outCardGame->priorityTextTasks->Allocate(outCardGame->arena, 32);
			outCardGame->pixelPerfectRenderTasks = &outCardGame->engine.AddTaskSystem<PixelPerfectRenderTask>();
			outCardGame->pixelPerfectRenderTasks->Allocate(outCardGame->arena, 128);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = jv::ge::GetResolution();

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = outCardGame->scene;
			enableInfo.capacity = 1024;

			outCardGame->priorityRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter<RenderTask>>(
				*outCardGame->priorityRenderTasks, createInfo);
			outCardGame->priorityRenderInterpreter->Enable(enableInfo);
			outCardGame->priorityRenderInterpreter->image = outCardGame->atlas;
			outCardGame->renderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter<RenderTask>>(
				*outCardGame->renderTasks, createInfo);
			outCardGame->renderInterpreter->Enable(enableInfo);
			outCardGame->renderInterpreter->image = outCardGame->atlas;

			DynamicRenderInterpreterCreateInfo dynamicCreateInfo{};
			dynamicCreateInfo.resolution = jv::ge::GetResolution();
			dynamicCreateInfo.frameArena = &mem.frameArena;

			DynamicRenderInterpreterEnableInfo dynamicEnableInfo{};
			dynamicEnableInfo.arena = &outCardGame->arena;
			dynamicEnableInfo.scene = outCardGame->scene;
			dynamicEnableInfo.capacity = 32;
			
			outCardGame->dynamicRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<DynamicRenderTask, DynamicRenderInterpreter>(
				*outCardGame->dynamicRenderTasks, dynamicCreateInfo);
			outCardGame->dynamicRenderInterpreter->Enable(dynamicEnableInfo);
			
			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.alphabetSubTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::alphabet)].subTexture;
			textInterpreterCreateInfo.symbolSubTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::symbols)].subTexture;
			textInterpreterCreateInfo.numberSubTexture = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::numbers)].subTexture;
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);

			textInterpreterCreateInfo.instancedRenderTasks = outCardGame->priorityRenderTasks;
			outCardGame->priorityTextInterpreter = &outCardGame->engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->priorityTextTasks, textInterpreterCreateInfo);
			textInterpreterCreateInfo.instancedRenderTasks = outCardGame->renderTasks;
			outCardGame->textInterpreter = &outCardGame->engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->textTasks, textInterpreterCreateInfo);

			PixelPerfectRenderInterpreterCreateInfo pixelPerfectRenderInterpreterCreateInfo{};
			pixelPerfectRenderInterpreterCreateInfo.renderTasks = outCardGame->renderTasks;
			pixelPerfectRenderInterpreterCreateInfo.priorityRenderTasks = outCardGame->priorityRenderTasks;
			pixelPerfectRenderInterpreterCreateInfo.resolution = jv::ge::GetResolution();
			pixelPerfectRenderInterpreterCreateInfo.simulatedResolution = SIMULATED_RESOLUTION;
			pixelPerfectRenderInterpreterCreateInfo.background = outCardGame->atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;

			outCardGame->pixelPerfectRenderInterpreter = &outCardGame->engine.AddTaskInterpreter<PixelPerfectRenderTask, PixelPerfectRenderInterpreter>(
				*outCardGame->pixelPerfectRenderTasks, pixelPerfectRenderInterpreterCreateInfo);
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
			outCardGame->levels = jv::CreateArray<Level*>(outCardGame->arena, 4);
			outCardGame->levels[0] = outCardGame->arena.New<MainMenuLevel>();
			outCardGame->levels[1] = outCardGame->arena.New<NewGameLevel>();
			outCardGame->levels[2] = outCardGame->arena.New<PartySelectLevel>();
			outCardGame->levels[3] = outCardGame->arena.New<MainLevel>();
		}
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
		arr[5] = "Art/card-field.png";
		arr[6] = "Art/card-mod.png";
		arr[7] = "Art/button.png";
		arr[8] = "Art/stats.png";
		arr[9] = "Art/fallback.png";
		arr[10] = "Art/empty.png";
		return arr;
	}

	jv::Array<MonsterCard> CardGame::GetMonsterCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MonsterCard>(arena, 30);
		for (auto& card : arr)
			card.name = "monster";
		// Starting pet.
		arr[0].unique = true;
		arr[0].name = "daisy, loyal protector";
		arr[0].ruleText = "follows you around.";
		arr[0].health = 999;
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, 10);
		for (auto& card : arr)
			card.name = "artifact";
		arr[0].unique = true;
		arr[0].name = "sword of a thousand truths";
		arr[0].ruleText = "whenever you attack, win the game.";
		return arr;
	}

	jv::Array<BossCard> CardGame::GetBossCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<BossCard>(arena, 10);
		for (auto& card : arr)
			card.name = "boss";
		return arr;
	}

	jv::Array<RoomCard> CardGame::GetRoomCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<RoomCard>(arena, 10);
		for (auto& card : arr)
			card.name = "room";
		return arr;
	}

	jv::Array<MagicCard> CardGame::GetMagicCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MagicCard>(arena, 24);
		for (auto& card : arr)
			card.name = "magic";
		return arr;
	}

	jv::Array<FlawCard> CardGame::GetFlawCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<FlawCard>(arena, 24);
		for (auto& card : arr)
			card.name = "flaw";
		return arr;
	}

	jv::Array<EventCard> CardGame::GetEventCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<EventCard>(arena, 24);
		for (auto& card : arr)
			card.name = "event";
		return arr;
	}

	void CardGame::UpdateInput()
	{
		const auto mousePos = jv::ge::GetMousePosition();

		inputState = {};
		inputState.mousePos = mousePos;
		inputState.scroll = scrollCallback;
		
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
				state = InputState::pressed;
			else if (callback.action == GLFW_RELEASE)
				state = InputState::released;
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
