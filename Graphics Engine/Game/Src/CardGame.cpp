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
#include "Tasks/MouseTask.h"
#include "Utils/BoxCollision.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

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

	void GetMonsterCardDeck(jv::Vector<uint32_t> monsterCardsDeck, const jv::Array<MonsterCard>& monsterCards, const PlayerState& playerState)
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
			auto& monsterCard = monsterCards[id];
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

		switch (_levelState)
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

	void CardGame::UpdateMainMenu()
	{
		const auto ptr = static_cast<MainMenuState*>(_levelStatePtr);

		MouseTask mouseTask;
		UpdateInput(mouseTask);

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
		remove(SAVE_DATA_PATH);
		GetMonsterCardDeck(_monsterCardsDeck, _monsterCards, _playerState);
	}

	void CardGame::UpdateNewGame()
	{
		DrawMonsterCard(0, glm::vec2(0));
		MouseTask mouseTask;
		UpdateInput(mouseTask);
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

	void CardGame::DrawMonsterCard(const uint32_t id, const glm::vec2 position) const
	{
		const auto& card = _monsterCards[id];
		constexpr float width = .3f;
		constexpr float height = .4f;
		constexpr float picFillPct = .6f;

		RenderTask bgRenderTask{};
		bgRenderTask.scale.y = height * (1.f - picFillPct);
		bgRenderTask.scale.x = width;
		bgRenderTask.position = position + glm::vec2(0, height * (1.f - picFillPct));
		bgRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		_renderTasks->Push(bgRenderTask);

		RenderTask picRenderTask{};
		picRenderTask.scale.y = height * picFillPct;
		picRenderTask.scale.x = width;
		picRenderTask.position = position - glm::vec2(0, height * picFillPct);
		picRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		picRenderTask.color = glm::vec4(1, 0, 0, 1);
		_renderTasks->Push(picRenderTask);

		TextTask titleTextTask{};
		titleTextTask.lineLength = 12;
		titleTextTask.center = true;
		titleTextTask.position = position - glm::vec2(0, height);
		titleTextTask.text = card.name;
		titleTextTask.scale = .04f;
		_textTasks->Push(titleTextTask);

		TextTask ruleTextTask = titleTextTask;
		ruleTextTask.position = position + glm::vec2(0, bgRenderTask.scale.y);
		ruleTextTask.text = card.ruleText;
		_textTasks->Push(ruleTextTask);
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
