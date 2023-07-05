#include "pch_game.h"
#include "CardGame.h"

#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>

#include "Cards/MonsterCard.h"
#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/MouseInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "States/MainMenuState.h"
#include "States/NewGameState.h"
#include "Tasks/MouseTask.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

	constexpr float CARD_SPACING = .1f;
	constexpr float CARD_WIDTH = .3f;
	constexpr float CARD_HEIGHT = .4f;
	constexpr float CARD_PIC_FILL_HEIGHT = .6f;
	constexpr float CARD_WIDTH_OFFSET = CARD_WIDTH * 2 + CARD_SPACING;
	constexpr float CARD_HEIGHT_OFFSET = CARD_HEIGHT + CARD_SPACING;
	constexpr float DISCOVER_LENGTH = 3;

	constexpr float CARD_DARKENED_COLOR_MUL = .2f;

	CardGame* userPtr = nullptr;

	jv::Array<const char*> GetTexturePaths(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<const char*>(arena, static_cast<uint32_t>(CardGame::TextureId::length));
		arr[0] = "Art/alphabet.png";
		arr[1] = "Art/numbers.png";
		arr[2] = "Art/symbols.png";
		arr[3] = "Art/mouse.png";
		arr[4] = "Art/fallback.png";
		return arr;
	}

	jv::Array<MonsterCard> GetMonsterCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<MonsterCard>(arena, 10);
		// Starting pet.
		arr[0].unique = true;
		arr[0].name = "daisy, loyal protector";
		arr[0].ruleText = "follows you around.";
		return arr;
	}

	jv::Array<ArtifactCard> GetArtifactCards(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<ArtifactCard>(arena, 10);
		arr[0].unique = true;
		arr[0].name = "sword of a thousand truths";
		arr[0].ruleText = "whenever you attack, win the game.";
		return arr;
	}

	void GetMonsterCardDeck(jv::Vector<uint32_t>& monsterCardsDeck, const jv::Array<MonsterCard>& monsterCards, const PlayerState& playerState)
	{
		monsterCardsDeck.Clear();
		for (uint32_t i = 0; i < monsterCardsDeck.length; ++i)
		{
			if (monsterCards[i].unique)
				continue;
			monsterCardsDeck.Add() = i;
		}

		for (auto i = static_cast<int32_t>(monsterCardsDeck.count) - 1; i >= 0; --i)
		{
			const uint32_t id = monsterCardsDeck[i];
			for (uint32_t j = 0; j < playerState.partySize; ++j)
			{
				if(playerState.monsterIds[j] == id)
				{
					monsterCardsDeck.RemoveAt(i);
					break;
				}
			}
		}
	}

	void GetArtifactCardDeck(jv::Vector<uint32_t>& artifactCardsDeck, const jv::Array<ArtifactCard>& artifactCards, const PlayerState& playerState)
	{
		artifactCardsDeck.Clear();
		for (uint32_t i = 0; i < artifactCardsDeck.length; ++i)
		{
			if (artifactCards[i].unique)
				continue;
			artifactCardsDeck.Add() = i;
		}

		for (auto i = static_cast<int32_t>(artifactCardsDeck.count) - 1; i >= 0; --i)
		{
			const uint32_t id = artifactCardsDeck[i];
			for (uint32_t j = 0; j < playerState.partySize; ++j)
			{
				const uint32_t artifactCount = playerState.artifactsCounts[j];
				for (uint32_t k = 0; k < artifactCount; ++k)
				{
					if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					{
						artifactCardsDeck.RemoveAt(i);
						break;
					}
				}
			}
		}
	}

	bool TryLoadSaveData(PlayerState& playerState)
	{
		std::ifstream inFile;
		if (!inFile.is_open())
			return false;
		inFile.open(SAVE_DATA_PATH);
		
		for (auto& monsterId : playerState.monsterIds)
			inFile >> monsterId;
		for (auto& health : playerState.healths)
			inFile >> health;
		for (auto& artifact : playerState.artifacts)
			inFile >> artifact;
		for (auto& artifactCount : playerState.artifactsCounts)
			inFile >> artifactCount;
		inFile >> playerState.partySize;

		return true;
	}

	void SaveData(PlayerState& playerState)
	{
		std::ofstream outFile;
		outFile.open(SAVE_DATA_PATH);

		for (const auto& monsterId : playerState.monsterIds)
			outFile << monsterId;
		for (const auto& health : playerState.healths)
			outFile << health;
		for (const auto& artifact : playerState.artifacts)
			outFile << artifact;
		for (const auto& artifactCount : playerState.artifactsCounts)
			outFile << artifactCount;
		outFile << playerState.partySize;
	}

	bool CardGame::Update()
	{
		if(_levelLoading)
		{
			if (++_levelLoadingFrame < jv::ge::GetFrameCount())
				return true;
			_levelLoadingFrame = 0;

			jv::ge::ClearScene(_levelScene);
			_levelArena.Clear();

			switch (_levelState)
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

			_levelLoading = false;
		}

		MouseTask mouseTask;
		UpdateInput(mouseTask);

		switch (_levelState)
		{
		case LevelState::mainMenu:
			UpdateMainMenu(mouseTask);
			break;
		case LevelState::newGame:
			UpdateNewGame(mouseTask);
			break;
		case LevelState::inGame:
			break;
		default:
			throw std::exception("Scene state not supported!");
		}

		const bool result = _engine.Update();
		return result;
	}

	void CardGame::Create(CardGame* outCardGame)
	{
		assert(!userPtr);
		userPtr = outCardGame;

		*outCardGame = {};
		{
			EngineCreateInfo engineCreateInfo{};
			engineCreateInfo.onKeyCallback = OnKeyCallback;
			engineCreateInfo.onMouseCallback = OnMouseCallback;
			engineCreateInfo.onScrollCallback = OnScrollCallback;
			outCardGame->_engine = Engine::Create(engineCreateInfo);
		}

		outCardGame->_arena = outCardGame->_engine.CreateSubArena(1024);
		outCardGame->_levelArena = outCardGame->_engine.CreateSubArena(1024);
		outCardGame->_scene = jv::ge::CreateScene();
		outCardGame->_levelScene = jv::ge::CreateScene();

		const auto mem = outCardGame->_engine.GetMemory();

#ifdef _DEBUG
		{
			const auto tempScope = mem.tempArena.CreateScope();

			const auto paths = GetTexturePaths(mem.tempArena);
			jv::ge::GenerateAtlas(outCardGame->_arena, mem.tempArena, paths,
				ATLAS_PATH, ATLAS_META_DATA_PATH);

			mem.tempArena.DestroyScope(tempScope);
		}
#endif

		int texWidth, texHeight, texChannels2;
		{
			stbi_uc* pixels = stbi_load(ATLAS_PATH, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

			jv::ge::ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.resolution = { texWidth, texHeight };
			imageCreateInfo.scene = outCardGame->_scene;
			outCardGame->_atlas = AddImage(imageCreateInfo);
			jv::ge::FillImage(outCardGame->_atlas, pixels);
			stbi_image_free(pixels);
			outCardGame->_subTextures = jv::ge::LoadAtlasMetaData(outCardGame->_arena, ATLAS_META_DATA_PATH);
		}

		{
			outCardGame->_renderTasks = &outCardGame->_engine.AddTaskSystem<RenderTask>();
			outCardGame->_renderTasks->Allocate(outCardGame->_arena, 1024);
			outCardGame->_dynamicRenderTasks = &outCardGame->_engine.AddTaskSystem<DynamicRenderTask>();
			outCardGame->_dynamicRenderTasks->Allocate(outCardGame->_arena, 32);
			outCardGame->_mouseTasks = &outCardGame->_engine.AddTaskSystem<MouseTask>();
			outCardGame->_mouseTasks->Allocate(outCardGame->_arena, 1);
			outCardGame->_textTasks = &outCardGame->_engine.AddTaskSystem<TextTask>();
			outCardGame->_textTasks->Allocate(outCardGame->_arena, 16);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = jv::ge::GetResolution();

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = outCardGame->_scene;
			enableInfo.capacity = 1024;

			outCardGame->_renderInterpreter = &outCardGame->_engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter>(
				*outCardGame->_renderTasks, createInfo);
			outCardGame->_renderInterpreter->Enable(enableInfo);
			outCardGame->_renderInterpreter->image = outCardGame->_atlas;

			DynamicRenderInterpreterCreateInfo dynamicCreateInfo{};
			dynamicCreateInfo.resolution = jv::ge::GetResolution();
			dynamicCreateInfo.frameArena = &mem.frameArena;

			DynamicRenderInterpreterEnableInfo dynamicEnableInfo{};
			dynamicEnableInfo.arena = &outCardGame->_arena;
			dynamicEnableInfo.scene = outCardGame->_scene;
			dynamicEnableInfo.capacity = 32;

			outCardGame->_dynamicRenderInterpreter = &outCardGame->_engine.AddTaskInterpreter<DynamicRenderTask, DynamicRenderInterpreter>(
				*outCardGame->_dynamicRenderTasks, dynamicCreateInfo);
			outCardGame->_dynamicRenderInterpreter->Enable(dynamicEnableInfo);

			MouseInterpreterCreateInfo mouseInterpreterCreateInfo{};
			mouseInterpreterCreateInfo.renderTasks = outCardGame->_renderTasks;
			outCardGame->mouseInterpreter = &outCardGame->_engine.AddTaskInterpreter<MouseTask, MouseInterpreter>(
				*outCardGame->_mouseTasks, mouseInterpreterCreateInfo);
			outCardGame->mouseInterpreter->subTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::mouse)];

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.instancedRenderTasks = outCardGame->_renderTasks;
			textInterpreterCreateInfo.alphabetSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.symbolSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
			outCardGame->_textInterpreter = &outCardGame->_engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->_textTasks, textInterpreterCreateInfo);
		}

		{
			outCardGame->_monsterCards = GetMonsterCards(outCardGame->_arena);
			outCardGame->_monsterCardsDeck = jv::CreateVector<uint32_t>(outCardGame->_arena, outCardGame->_monsterCards.length);
			outCardGame->_artifactCards = GetArtifactCards(outCardGame->_arena);
			outCardGame->_artifactCardsDeck = jv::CreateVector<uint32_t>(outCardGame->_arena, outCardGame->_artifactCards.length);
		}
	}

	void CardGame::Destroy(const CardGame& cardGame)
	{
		assert(userPtr);
		userPtr = nullptr;

		jv::Arena::Destroy(cardGame._arena);
		Engine::Destroy(cardGame._engine);
	}

	void CardGame::LoadMainMenu()
	{
		const auto ptr = _arena.New<MainMenuState>();
		_levelStatePtr = ptr;
		ptr->saveDataValid = TryLoadSaveData(_playerState);
	}

	void CardGame::UpdateMainMenu(const MouseTask& mouseTask)
	{
		const auto ptr = static_cast<MainMenuState*>(_levelStatePtr);
		
		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.center = true;
		titleTextTask.position.y = -0.75f;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = .12f;
		_textTasks->Push(titleTextTask);

		const float buttonYOffset = ptr->saveDataValid ? -.18 : 0;

		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = buttonYOffset;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.scale.x = .4f;
		buttonRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		_renderTasks->Push(buttonRenderTask);
		
		if (mouseTask.lButton == MouseTask::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, mouseTask.position))
			{
				_levelState = LevelState::newGame;
				_levelLoading = true;
			}

		TextTask buttonTextTask{};
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "new game";
		buttonTextTask.scale = .06f;
		_textTasks->Push(buttonTextTask);

		if(ptr->saveDataValid)
		{
			buttonRenderTask.position.y *= -1;
			_renderTasks->Push(buttonRenderTask);

			buttonTextTask.position = buttonRenderTask.position;
			buttonTextTask.text = "continue";
			_textTasks->Push(buttonTextTask);

			if (mouseTask.lButton == MouseTask::pressed)
				if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, mouseTask.position))
				{
					_levelState = LevelState::inGame;
					_levelLoading = true;
				}
		}
	}

	void CardGame::LoadNewGame()
	{
		const auto ptr = _arena.New<NewGameState>();
		_levelStatePtr = ptr;

		remove(SAVE_DATA_PATH);
		GetMonsterCardDeck(_monsterCardsDeck, _monsterCards, _playerState);
		GetArtifactCardDeck(_artifactCardsDeck, _artifactCards, _playerState);

		// Create a discover option for your initial monster.
		ptr->monsterDiscoverOptions = jv::CreateArray<uint32_t>(_arena, DISCOVER_LENGTH);
		Shuffle(_monsterCardsDeck.ptr, _monsterCardsDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			ptr->monsterDiscoverOptions[i] = _monsterCardsDeck.Pop();
		// Create a discover option for your initial artifact.
		ptr->artifactDiscoverOptions = jv::CreateArray<uint32_t>(_arena, DISCOVER_LENGTH);
		Shuffle(_artifactCardsDeck.ptr, _artifactCardsDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			ptr->artifactDiscoverOptions[i] = _artifactCardsDeck.Pop();
	}

	void CardGame::UpdateNewGame(const MouseTask& mouseTask)
	{
		const auto ptr = static_cast<NewGameState*>(_levelStatePtr);

		auto choice = DrawMonsterChoice(ptr->monsterDiscoverOptions.ptr, glm::vec2(0, -CARD_HEIGHT_OFFSET), DISCOVER_LENGTH, ptr->monsterChoice);
		if (mouseTask.lButton == MouseTask::pressed && choice != -1)
			ptr->monsterChoice = choice;
		choice = DrawArtifactChoice(ptr->artifactDiscoverOptions.ptr, glm::vec2(0, CARD_HEIGHT_OFFSET), DISCOVER_LENGTH, ptr->artifactChoice);
		if (mouseTask.lButton == MouseTask::pressed && choice != -1)
			ptr->artifactChoice = choice;
	}

	void CardGame::UpdateInput(MouseTask& outMouseTask)
	{
		outMouseTask = {};
		outMouseTask.position = GetConvertedMousePosition();
		outMouseTask.scroll = _scrollCallback;

		for (const auto& callback : _mouseCallbacks)
		{
			if (callback.key == GLFW_MOUSE_BUTTON_LEFT && callback.action == GLFW_PRESS)
				outMouseTask.lButton = MouseTask::pressed;
			if (callback.key == GLFW_MOUSE_BUTTON_RIGHT && callback.action == GLFW_PRESS)
				outMouseTask.rButton = MouseTask::pressed;
			if (callback.key == GLFW_MOUSE_BUTTON_LEFT && callback.action == GLFW_RELEASE)
				outMouseTask.lButton = MouseTask::released;
			if (callback.key == GLFW_MOUSE_BUTTON_RIGHT && callback.action == GLFW_RELEASE)
				outMouseTask.rButton = MouseTask::released;
		}
		_mouseTasks->Push(outMouseTask);

		// Reset callbacks.
		_keyCallbacks = {};
		_mouseCallbacks = {};
		_scrollCallback = 0;
	}

	void CardGame::DrawMonsterCard(const uint32_t id, const glm::vec2 position, const glm::vec4 color) const
	{
		const auto& card = _monsterCards[id];

		RenderTask bgRenderTask{};
		bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
		bgRenderTask.scale.x = CARD_WIDTH;
		bgRenderTask.position = position + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
		bgRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		bgRenderTask.color = color;
		_renderTasks->Push(bgRenderTask);

		RenderTask picRenderTask{};
		picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
		picRenderTask.scale.x = CARD_WIDTH;
		picRenderTask.position = position - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
		picRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		picRenderTask.color = color;
		_renderTasks->Push(picRenderTask);

		TextTask titleTextTask{};
		titleTextTask.lineLength = 12;
		titleTextTask.center = true;
		titleTextTask.position = position - glm::vec2(0, CARD_HEIGHT);
		titleTextTask.text = card.name;
		titleTextTask.scale = .04f;
		_textTasks->Push(titleTextTask);

		TextTask ruleTextTask = titleTextTask;
		ruleTextTask.position = position + glm::vec2(0, bgRenderTask.scale.y);
		ruleTextTask.text = card.ruleText;
		_textTasks->Push(ruleTextTask);
	}

	uint32_t CardGame::DrawMonsterChoice(const uint32_t* ids, const glm::vec2 center, const uint32_t count, const uint32_t highlight) const
	{
		const float offset = -CARD_WIDTH_OFFSET * (count - 1) / 2;
		const auto mousePos = GetConvertedMousePosition();
		uint32_t selected = -1;
		const auto defaultColor = glm::vec4(1) * (highlight < count ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto pos = center + glm::vec2(offset + CARD_WIDTH_OFFSET * static_cast<float>(i), 0);
			DrawMonsterCard(ids[i], pos, highlight == i ? glm::vec4(1) : defaultColor);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), mousePos))
				selected = i;
		}

		return selected;
	}

	void CardGame::DrawArtifactCard(const uint32_t id, const glm::vec2 position, const glm::vec4 color) const
	{
		const auto& card = _artifactCards[id];

		RenderTask bgRenderTask{};
		bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
		bgRenderTask.scale.x = CARD_WIDTH;
		bgRenderTask.position = position + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
		bgRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		bgRenderTask.color = color;
		_renderTasks->Push(bgRenderTask);

		RenderTask picRenderTask{};
		picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
		picRenderTask.scale.x = CARD_WIDTH;
		picRenderTask.position = position - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
		picRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		picRenderTask.color = color;
		_renderTasks->Push(picRenderTask);

		TextTask titleTextTask{};
		titleTextTask.lineLength = 12;
		titleTextTask.center = true;
		titleTextTask.position = position - glm::vec2(0, CARD_HEIGHT);
		titleTextTask.text = card.name;
		titleTextTask.scale = .04f;
		_textTasks->Push(titleTextTask);

		TextTask ruleTextTask = titleTextTask;
		ruleTextTask.position = position + glm::vec2(0, bgRenderTask.scale.y);
		ruleTextTask.text = card.ruleText;
		_textTasks->Push(ruleTextTask);
	}

	uint32_t CardGame::DrawArtifactChoice(const uint32_t* ids, const glm::vec2 center, const uint32_t count, const uint32_t highlight) const
	{
		const float offset = -CARD_WIDTH_OFFSET * (count - 1) / 2;
		const auto mousePos = GetConvertedMousePosition();
		uint32_t selected = -1;
		const auto defaultColor = glm::vec4(1) * (highlight < count ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto pos = center + glm::vec2(offset + CARD_WIDTH_OFFSET * static_cast<float>(i), 0);
			DrawArtifactCard(ids[i], pos, highlight == i ? glm::vec4(1) : defaultColor);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), mousePos))
				selected = i;
		}

		return selected;
	}

	void CardGame::OnKeyCallback(const size_t key, const size_t action)
	{
		KeyCallback callback{};
		callback.key = key;
		callback.action = action;
		const auto mem = userPtr->_engine.GetMemory();
		Add(mem.frameArena, userPtr->_keyCallbacks) = callback;
	}

	void CardGame::OnMouseCallback(const size_t key, const size_t action)
	{
		KeyCallback callback{};
		callback.key = key;
		callback.action = action;
		const auto mem = userPtr->_engine.GetMemory();
		Add(mem.frameArena, userPtr->_mouseCallbacks) = callback;
	}

	void CardGame::OnScrollCallback(const glm::vec<2, double> offset)
	{
		userPtr->_scrollCallback += static_cast<float>(offset.y);
	}

	glm::vec2 CardGame::GetConvertedMousePosition()
	{
		const auto mousePos = jv::ge::GetMousePosition();
		const auto resolution = jv::ge::GetResolution();
		auto cPos = glm::vec2(mousePos.x, mousePos.y) / glm::vec2(resolution.x, resolution.y);
		cPos *= 2;
		cPos -= glm::vec2(1, 1);
		cPos.x *= static_cast<float>(resolution.x) / static_cast<float>(resolution.y);
		return cPos;
	}
}
