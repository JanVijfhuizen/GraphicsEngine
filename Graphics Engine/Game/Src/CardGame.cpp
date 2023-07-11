#include "pch_game.h"
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
#include "Interpreters/MouseInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "Levels/Level.h"
#include "Levels/LevelUtils.h"
#include "Levels/MainLevel.h"
#include "Levels/MainMenuLevel.h"
#include "Levels/NewGameLevel.h"
#include "Levels/PartySelectLevel.h"
#include "States/BoardState.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Tasks/MouseTask.h"

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
		jv::Array<jv::ge::SubTexture> subTextures;
		TaskSystem<RenderTask>* renderTasks;
		TaskSystem<DynamicRenderTask>* dynamicRenderTasks;
		TaskSystem<MouseTask>* mouseTasks;
		TaskSystem<TextTask>* textTasks;
		InstancedRenderInterpreter* renderInterpreter;
		DynamicRenderInterpreter* dynamicRenderInterpreter;
		MouseInterpreter* mouseInterpreter;
		TextInterpreter* textInterpreter;

		GameState gameState{};
		PlayerState playerState{};
		BoardState boardState{};

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

		jv::Vector<uint32_t> monsterDeck;
		jv::Vector<uint32_t> artifactDeck;

		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(const CardGame& cardGame);

		[[nodiscard]] static jv::Array<const char*> GetTexturePaths(jv::Arena& arena);
		[[nodiscard]] static jv::Array<MonsterCard> GetMonsterCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<ArtifactCard> GetArtifactCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<BossCard> GetBossCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<RoomCard> GetRoomCards(jv::Arena& arena);
		[[nodiscard]] static jv::Array<MagicCard> GetMagicCards(jv::Arena& arena);
		
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
				gameState,
				playerState,
				boardState,
				monsters,
				artifacts,
				bosses,
				rooms,
				magic,
				monsterDeck,
				artifactDeck
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
			gameState,
			playerState,
			boardState,
			monsters,
			artifacts,
			bosses,
			rooms,
			magic,
			monsterDeck,
			artifactDeck,
			inputState,
			*renderTasks,
			*dynamicRenderTasks,
			*textTasks,
			subTextures
		};

		auto loadLevelIndex = levelIndex;
		auto result = levels[static_cast<uint32_t>(levelIndex)]->Update(info, loadLevelIndex);
		if (!result)
			return result;

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
			outCardGame->subTextures = jv::ge::LoadAtlasMetaData(outCardGame->arena, ATLAS_META_DATA_PATH);
		}

		{
			outCardGame->renderTasks = &outCardGame->engine.AddTaskSystem<RenderTask>();
			outCardGame->renderTasks->Allocate(outCardGame->arena, 1024);
			outCardGame->dynamicRenderTasks = &outCardGame->engine.AddTaskSystem<DynamicRenderTask>();
			outCardGame->dynamicRenderTasks->Allocate(outCardGame->arena, 32);
			outCardGame->mouseTasks = &outCardGame->engine.AddTaskSystem<MouseTask>();
			outCardGame->mouseTasks->Allocate(outCardGame->arena, 1);
			outCardGame->textTasks = &outCardGame->engine.AddTaskSystem<TextTask>();
			outCardGame->textTasks->Allocate(outCardGame->arena, 16);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = jv::ge::GetResolution();

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = outCardGame->scene;
			enableInfo.capacity = 1024;

			outCardGame->renderInterpreter = &outCardGame->engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter>(
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

			MouseInterpreterCreateInfo mouseInterpreterCreateInfo{};
			mouseInterpreterCreateInfo.renderTasks = outCardGame->renderTasks;
			outCardGame->mouseInterpreter = &outCardGame->engine.AddTaskInterpreter<MouseTask, MouseInterpreter>(
				*outCardGame->mouseTasks, mouseInterpreterCreateInfo);
			outCardGame->mouseInterpreter->subTexture = outCardGame->subTextures[static_cast<uint32_t>(TextureId::mouse)];

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.instancedRenderTasks = outCardGame->renderTasks;
			textInterpreterCreateInfo.alphabetSubTexture = outCardGame->subTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.symbolSubTexture = outCardGame->subTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberSubTexture = outCardGame->subTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
			outCardGame->textInterpreter = &outCardGame->engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->textTasks, textInterpreterCreateInfo);
		}

		{
			outCardGame->monsters = cardGame.GetMonsterCards(outCardGame->arena);
			outCardGame->artifacts = cardGame.GetArtifactCards(outCardGame->arena);
			outCardGame->bosses = cardGame.GetBossCards(outCardGame->arena);
			outCardGame->rooms = cardGame.GetRoomCards(outCardGame->arena);
			outCardGame->magic = cardGame.GetMagicCards(outCardGame->arena);

			uint32_t count;
			GetDeck(nullptr, &count, outCardGame->monsters);
			outCardGame->monsterDeck = jv::CreateVector<uint32_t>(outCardGame->arena, count);
			GetDeck(nullptr, &count, outCardGame->artifacts);
			outCardGame->artifactDeck = jv::CreateVector<uint32_t>(outCardGame->arena, count);
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
		arr[4] = "Art/fallback.png";
		return arr;
	}

	jv::Array<MonsterCard> CardGame::GetMonsterCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MonsterCard>(arena, 10);
		// Starting pet.
		arr[0].unique = true;
		arr[0].name = "daisy, loyal protector";
		arr[0].ruleText = "follows you around.";
		return arr;
	}

	jv::Array<ArtifactCard> CardGame::GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, 10);
		arr[0].unique = true;
		arr[0].name = "sword of a thousand truths";
		arr[0].ruleText = "whenever you attack, win the game.";
		return arr;
	}

	jv::Array<BossCard> CardGame::GetBossCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<BossCard>(arena, 10);
		arr[0].name = "ivern the cruel";
		arr[0].ruleText = "summon ivern and daisy.";
		return arr;
	}

	jv::Array<RoomCard> CardGame::GetRoomCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<RoomCard>(arena, 10);
		return arr;
	}

	jv::Array<MagicCard> CardGame::GetMagicCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MagicCard>(arena, 24);
		return arr;
	}

	void CardGame::UpdateInput()
	{
		const auto mousePos = jv::ge::GetMousePosition();
		const auto resolution = jv::ge::GetResolution();
		auto cPos = glm::vec2(mousePos.x, mousePos.y) / glm::vec2(resolution.x, resolution.y);
		cPos *= 2;
		cPos -= glm::vec2(1, 1);
		cPos.x *= static_cast<float>(resolution.x) / static_cast<float>(resolution.y);

		inputState = {};
		inputState.mousePos = cPos;
		inputState.scroll = scrollCallback;
		
		for (const auto& callback : mouseCallbacks)
		{
			SetInputState(inputState.lMouse, GLFW_MOUSE_BUTTON_LEFT, callback);
			SetInputState(inputState.rMouse, GLFW_MOUSE_BUTTON_RIGHT, callback);
		}
		for (const auto& callback : keyCallbacks)
			SetInputState(inputState.enter, GLFW_KEY_ENTER, callback);

		MouseTask mouseTask{};
		mouseTask.position = inputState.mousePos;
		mouseTask.lButton = inputState.lMouse;
		mouseTask.rButton = inputState.rMouse;
		mouseTask.scroll = inputState.scroll;
		mouseTasks->Push(mouseTask);

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
		for (auto& health : playerState.healths)
			inFile >> health;
		for (auto& artifact : playerState.artifacts)
			inFile >> artifact;
		for (auto& artifactCount : playerState.artifactsCounts)
			inFile >> artifactCount;
		inFile >> playerState.partySize;
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
		for (const auto& health : playerState.healths)
			outFile << health << std::endl;
		for (const auto& artifact : playerState.artifacts)
			outFile << artifact << std::endl;
		for (const auto& artifactCount : playerState.artifactsCounts)
			outFile << artifactCount << std::endl;
		outFile << playerState.partySize << std::endl;
		outFile.close();
	}

	void ClearSaveData()
	{
		assert(cardGameRunning);
		remove(SAVE_DATA_PATH);
	}
}
