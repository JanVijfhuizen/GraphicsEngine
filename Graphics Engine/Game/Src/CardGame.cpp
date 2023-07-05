﻿#include "pch_game.h"
#include "CardGame.h"

#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>

#include "Cards/ArtifactCard.h"
#include "Cards/MonsterCard.h"
#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/MouseInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "States/BoardState.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/MainMenuState.h"
#include "States/NewGameState.h"
#include "States/PlayerState.h"
#include "Tasks/MouseTask.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

	constexpr float CARD_SPACING = .1f;
	constexpr float CARD_WIDTH = .15f;
	constexpr float CARD_HEIGHT = .2f;
	constexpr float CARD_PIC_FILL_HEIGHT = .6f;
	constexpr float CARD_WIDTH_OFFSET = CARD_WIDTH * 2 + CARD_SPACING;
	constexpr float CARD_HEIGHT_OFFSET = CARD_HEIGHT + CARD_SPACING;
	constexpr float CARD_TEXT_SIZE = CARD_WIDTH * .1f;
	constexpr float DISCOVER_LENGTH = 3;

	constexpr float CARD_DARKENED_COLOR_MUL = .2f;

	enum class TextureId
	{
		alphabet,
		numbers,
		symbols,
		mouse,
		fallback,
		length
	};

	enum class LevelState
	{
		mainMenu,
		newGame,
		inGame
	};

	struct KeyCallback final
	{
		uint32_t key;
		uint32_t action;
	};
	
	struct CardGame final
	{
		InputState inputState{};

		[[nodiscard]] bool Update();
		static void Create(CardGame* outCardGame);
		static void Destroy(const CardGame& cardGame);

		static jv::Array<const char*> GetTexturePaths(jv::Arena& arena);
		static jv::Array<MonsterCard> GetMonsterCards(jv::Arena& arena);
		static jv::Array<ArtifactCard> GetArtifactCards(jv::Arena& arena);

		template <typename T>
		static void GetDeck(jv::Vector<uint32_t>& outDeck, const jv::Array<T>& cards, const PlayerState& playerState, bool(*func)(uint32_t, const PlayerState&));
		[[nodiscard]] static bool ValidateMonsterInclusion(uint32_t id, const PlayerState& playerState);
		[[nodiscard]] static bool ValidateArtifactInclusion(uint32_t id, const PlayerState& playerState);

		static bool TryLoadSaveData(PlayerState& playerState);
		static void SaveData(PlayerState& playerState);

		void LoadMainMenu();
		void UpdateMainMenu();
		void LoadNewGame();
		void UpdateNewGame();
		
		void UpdateInput();
		static void SetInputState(InputState::State& state, uint32_t target, KeyCallback callback);
		void DrawMonsterCard(uint32_t id, glm::vec2 position, glm::vec4 color = glm::vec4(1)) const;
		[[nodiscard]] uint32_t DrawMonsterChoice(const uint32_t* ids, glm::vec2 center, uint32_t count, uint32_t highlight = -1) const;
		void DrawArtifactCard(uint32_t id, glm::vec2 position, glm::vec4 color = glm::vec4(1)) const;
		[[nodiscard]] uint32_t DrawArtifactChoice(const uint32_t* ids, glm::vec2 center, uint32_t count, uint32_t highlight = -1) const;

		static void OnKeyCallback(size_t key, size_t action);
		static void OnMouseCallback(size_t key, size_t action);
		static void OnScrollCallback(glm::vec<2, double> offset);

		Engine engine;
		jv::Arena arena;
		jv::Arena levelArena;
		jv::ge::Resource scene;
		jv::ge::Resource levelScene;
		jv::ge::Resource atlas;
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

		LevelState levelState = LevelState::mainMenu;
		bool levelLoading = true;
		uint32_t levelLoadingFrame = 0;
		void* levelStatePtr;

		jv::LinkedList<KeyCallback> keyCallbacks{};
		jv::LinkedList<KeyCallback> mouseCallbacks{};
		float scrollCallback = 0;

		jv::Array<MonsterCard> monsters;
		jv::Array<ArtifactCard> artifacts;

		jv::Vector<uint32_t> monsterDeck;
		jv::Vector<uint32_t> artifactDeck;
	} cardGame{};

	template <typename T>
	void CardGame::GetDeck(jv::Vector<uint32_t>& outDeck, const jv::Array<T>& cards, const PlayerState& playerState,
		bool(* func)(uint32_t, const PlayerState&))
	{
		outDeck.Clear();
		for (uint32_t i = 0; i < outDeck.length; ++i)
		{
			if (cards[i].unique)
				continue;
			if (!func(i, playerState))
				continue;
			outDeck.Add() = i;
		}
	}

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

			switch (levelState)
			{
				case LevelState::mainMenu:
					LoadMainMenu();
					break;
				case LevelState::newGame:
					LoadNewGame();
					break;
				case LevelState::inGame:
					break;
				default:
					throw std::exception("Scene state not supported!");
			}

			levelLoading = false;
		}

		switch (levelState)
		{
		case LevelState::mainMenu:
			UpdateMainMenu();
			break;
		case LevelState::newGame:
			UpdateNewGame();
			break;
		case LevelState::inGame:
			break;
		default:
			throw std::exception("Scene state not supported!");
		}

		const bool result = engine.Update();
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
			outCardGame->monsterDeck = jv::CreateVector<uint32_t>(outCardGame->arena, outCardGame->monsters.length);
			outCardGame->artifacts = cardGame.GetArtifactCards(outCardGame->arena);
			outCardGame->artifactDeck = jv::CreateVector<uint32_t>(outCardGame->arena, outCardGame->artifacts.length);
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

	bool CardGame::ValidateMonsterInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
			if (playerState.monsterIds[j] == id)
				return false;
		return true;
	}

	bool CardGame::ValidateArtifactInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
		{
			const uint32_t artifactCount = playerState.artifactsCounts[j];
			for (uint32_t k = 0; k < artifactCount; ++k)
				if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					return false;
		}
		return true;
	}

	bool CardGame::TryLoadSaveData(PlayerState& playerState)
	{
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

	void CardGame::SaveData(PlayerState& playerState)
	{
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

	void CardGame::LoadMainMenu()
	{
		const auto ptr = arena.New<MainMenuState>();
		levelStatePtr = ptr;
		ptr->saveDataValid = TryLoadSaveData(playerState);
	}

	void CardGame::UpdateMainMenu()
	{
		const auto ptr = static_cast<MainMenuState*>(levelStatePtr);
		
		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.center = true;
		titleTextTask.position.y = -0.75f;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = .12f;
		textTasks->Push(titleTextTask);

		const float buttonYOffset = ptr->saveDataValid ? -.18 : 0;

		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = buttonYOffset;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.scale.x = .4f;
		buttonRenderTask.subTexture = subTextures[static_cast<uint32_t>(TextureId::fallback)];
		renderTasks->Push(buttonRenderTask);
		
		if (inputState.lMouse == InputState::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, inputState.mousePos))
			{
				levelState = LevelState::newGame;
				levelLoading = true;
			}

		TextTask buttonTextTask{};
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "new game";
		buttonTextTask.scale = .06f;
		textTasks->Push(buttonTextTask);

		if(ptr->saveDataValid)
		{
			buttonRenderTask.position.y *= -1;
			renderTasks->Push(buttonRenderTask);

			buttonTextTask.position = buttonRenderTask.position;
			buttonTextTask.text = "continue";
			textTasks->Push(buttonTextTask);

			if (inputState.lMouse == InputState::pressed)
				if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, inputState.mousePos))
				{
					levelState = LevelState::inGame;
					levelLoading = true;
				}
		}
	}

	void CardGame::LoadNewGame()
	{
		const auto ptr = arena.New<NewGameState>();
		levelStatePtr = ptr;

		remove(SAVE_DATA_PATH);
		GetDeck(monsterDeck, monsters, playerState, ValidateMonsterInclusion);
		GetDeck(artifactDeck, artifacts, playerState, ValidateArtifactInclusion);

		// Create a discover option for your initial monster.
		ptr->monsterDiscoverOptions = jv::CreateArray<uint32_t>(arena, DISCOVER_LENGTH);
		Shuffle(monsterDeck.ptr, monsterDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			ptr->monsterDiscoverOptions[i] = monsterDeck.Pop();
		// Create a discover option for your initial artifact.
		ptr->artifactDiscoverOptions = jv::CreateArray<uint32_t>(arena, DISCOVER_LENGTH);
		Shuffle(artifactDeck.ptr, artifactDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			ptr->artifactDiscoverOptions[i] = artifactDeck.Pop();
	}

	void CardGame::UpdateNewGame()
	{
		const auto ptr = static_cast<NewGameState*>(levelStatePtr);

		if(!ptr->confirmedChoices)
		{
			auto choice = DrawMonsterChoice(ptr->monsterDiscoverOptions.ptr, glm::vec2(0, -CARD_HEIGHT_OFFSET), DISCOVER_LENGTH, ptr->monsterChoice);
			if (inputState.lMouse == InputState::pressed && choice != -1)
				ptr->monsterChoice = choice == ptr->monsterChoice ? -1 : choice;
			choice = DrawArtifactChoice(ptr->artifactDiscoverOptions.ptr, glm::vec2(0, CARD_HEIGHT_OFFSET), DISCOVER_LENGTH, ptr->artifactChoice);
			if (inputState.lMouse == InputState::pressed && choice != -1)
				ptr->artifactChoice = choice == ptr->artifactChoice ? -1 : choice;

			TextTask buttonTextTask{};
			buttonTextTask.center = true;
			buttonTextTask.text = "choose your starting cards.";
			buttonTextTask.position = glm::vec2(0, -.8f);
			buttonTextTask.scale = .06f;
			textTasks->Push(buttonTextTask);

			if (ptr->monsterChoice != -1 && ptr->artifactChoice != -1)
			{
				TextTask textTask{};
				textTask.center = true;
				textTask.text = "press enter to confirm your choice.";
				textTask.position = glm::vec2(0, .8f);
				textTask.scale = .06f;
				textTasks->Push(textTask);

				if (inputState.enter == InputState::pressed)
					ptr->confirmedChoices = true;
			}
			return;
		}

		TextTask joinTextTask{};
		joinTextTask.center = true;
		joinTextTask.text = "daisy joins you on your adventure.";
		joinTextTask.position = glm::vec2(0, -.8f);
		joinTextTask.scale = .06f;
		textTasks->Push(joinTextTask);
		DrawMonsterCard(0, glm::vec2(0));

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "press enter to continue.";
		textTask.scale = .06f;
		textTask.position = glm::vec2(0, .8f);
		textTasks->Push(textTask);

		if (inputState.enter == InputState::pressed)
		{
			playerState.monsterIds[0] = ptr->monsterDiscoverOptions[ptr->monsterChoice];
			playerState.artifactsCounts[0] = 1;
			playerState.artifacts[0] = ptr->artifactDiscoverOptions[ptr->artifactChoice];
			playerState.monsterIds[1] = 0;
			playerState.partySize = 2;
			SaveData(playerState);
			levelState = LevelState::inGame;
			levelLoading = true;
		}
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

	void CardGame::DrawMonsterCard(const uint32_t id, const glm::vec2 position, const glm::vec4 color) const
	{
		const auto& card = monsters[id];

		RenderTask bgRenderTask{};
		bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
		bgRenderTask.scale.x = CARD_WIDTH;
		bgRenderTask.position = position + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
		bgRenderTask.subTexture = subTextures[static_cast<uint32_t>(TextureId::fallback)];
		bgRenderTask.color = color;
		renderTasks->Push(bgRenderTask);

		RenderTask picRenderTask{};
		picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
		picRenderTask.scale.x = CARD_WIDTH;
		picRenderTask.position = position - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
		picRenderTask.subTexture = subTextures[static_cast<uint32_t>(TextureId::fallback)];
		picRenderTask.color = color;
		renderTasks->Push(picRenderTask);

		TextTask titleTextTask{};
		titleTextTask.lineLength = 12;
		titleTextTask.center = true;
		titleTextTask.position = position - glm::vec2(0, CARD_HEIGHT);
		titleTextTask.text = card.name;
		titleTextTask.scale = CARD_TEXT_SIZE;
		textTasks->Push(titleTextTask);

		TextTask ruleTextTask = titleTextTask;
		ruleTextTask.position = position + glm::vec2(0, bgRenderTask.scale.y);
		ruleTextTask.text = card.ruleText;
		textTasks->Push(ruleTextTask);
	}

	uint32_t CardGame::DrawMonsterChoice(const uint32_t* ids, const glm::vec2 center, const uint32_t count, const uint32_t highlight) const
	{
		const float offset = -CARD_WIDTH_OFFSET * (count - 1) / 2;
		uint32_t selected = -1;
		const auto defaultColor = glm::vec4(1) * (highlight < count ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto pos = center + glm::vec2(offset + CARD_WIDTH_OFFSET * static_cast<float>(i), 0);
			DrawMonsterCard(ids[i], pos, highlight == i ? glm::vec4(1) : defaultColor);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), inputState.mousePos))
				selected = i;
		}

		return selected;
	}

	void CardGame::DrawArtifactCard(const uint32_t id, const glm::vec2 position, const glm::vec4 color) const
	{
		const auto& card = artifacts[id];

		RenderTask bgRenderTask{};
		bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
		bgRenderTask.scale.x = CARD_WIDTH;
		bgRenderTask.position = position + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
		bgRenderTask.subTexture = subTextures[static_cast<uint32_t>(TextureId::fallback)];
		bgRenderTask.color = color;
		renderTasks->Push(bgRenderTask);

		RenderTask picRenderTask{};
		picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
		picRenderTask.scale.x = CARD_WIDTH;
		picRenderTask.position = position - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
		picRenderTask.subTexture = subTextures[static_cast<uint32_t>(TextureId::fallback)];
		picRenderTask.color = color;
		renderTasks->Push(picRenderTask);

		TextTask titleTextTask{};
		titleTextTask.lineLength = 12;
		titleTextTask.center = true;
		titleTextTask.position = position - glm::vec2(0, CARD_HEIGHT);
		titleTextTask.text = card.name;
		titleTextTask.scale = CARD_TEXT_SIZE;
		textTasks->Push(titleTextTask);

		TextTask ruleTextTask = titleTextTask;
		ruleTextTask.position = position + glm::vec2(0, bgRenderTask.scale.y);
		ruleTextTask.text = card.ruleText;
		textTasks->Push(ruleTextTask);
	}

	uint32_t CardGame::DrawArtifactChoice(const uint32_t* ids, const glm::vec2 center, const uint32_t count, const uint32_t highlight) const
	{
		const float offset = -CARD_WIDTH_OFFSET * (count - 1) / 2;
		uint32_t selected = -1;
		const auto defaultColor = glm::vec4(1) * (highlight < count ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto pos = center + glm::vec2(offset + CARD_WIDTH_OFFSET * static_cast<float>(i), 0);
			DrawArtifactCard(ids[i], pos, highlight == i ? glm::vec4(1) : defaultColor);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), inputState.mousePos))
				selected = i;
		}

		return selected;
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
}
