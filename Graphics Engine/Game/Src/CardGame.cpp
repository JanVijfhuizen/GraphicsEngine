#include "pch_game.h"
#include "CardGame.h"

#include <fstream>
#include <stb_image.h>
#include <Engine/Engine.h>

#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"
#include "States/MainMenuState.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";
	constexpr const char* SAVE_DATA_PATH = "SaveData.txt";

	jv::Array<const char*> GetTexturePaths(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<const char*>(arena, static_cast<uint32_t>(CardGame::TextureId::length));
		arr[0] = "Art/alphabet.png";
		arr[1] = "Art/numbers.png";
		arr[2] = "Art/symbols.png";
		arr[3] = "Art/fallback.png";
		return arr;
	}

	bool TryLoadSaveData(PlayerState& playerState)
	{
		std::ifstream inFile;
		if (!inFile.good())
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
			switch (_sceneState)
			{
				case LevelState::mainMenu:
					LoadMainMenu();
					break;
				case LevelState::newGame: 
					break;
				case LevelState::inGame:
					break;
				default:
					throw std::exception("Scene state not supported!");
			}

			_levelLoading = false;
		}

		switch (_sceneState)
		{
		case LevelState::mainMenu:
			UpdateMainMenu();
			break;
		case LevelState::newGame:
			break;
		case LevelState::inGame:
			break;
		default:
			throw std::exception("Scene state not supported!");
		}

		/*
		static float f = 0;
		f += 0.01f;

		for (uint32_t i = 0; i < 5; ++i)
		{
			constexpr float dist = .2f;
			RenderTask renderTask{};
			renderTask.position.x = sin(f + dist * i) * .25f;
			renderTask.position.y = cos(f + dist * i) * .25f;
			renderTask.position *= 1.f + (5.f - static_cast<float>(i)) * .2f;
			renderTask.color.r *= static_cast<float>(i) / 5;
			renderTask.scale *= .2f;
			renderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
			_renderTasks->Push(renderTask);
		}

		TextTask textTask{};
		textTask.text = "just a test.";
		textTask.position.x = -1;
		textTask.position.y = -.75f;
		_textTasks->Push(textTask);

		auto& camera = _renderInterpreter->camera;
		camera.position.x = abs(sin(f / 3)) * .2f;
		camera.zoom = -.4f + abs(cos(f / 2)) * .2f;
		camera.rotation = sin(f / 4) * .2f;

		DynamicRenderTask dynRenderTask{};
		dynRenderTask.renderTask.scale *= .2f;
		dynRenderTask.renderTask.color.y *= .5f + sin(f) * .5f;
		_dynamicRenderTasks->Push(dynRenderTask);
		*/

		const bool result = _engine.Update();
		return result;
	}

	void CardGame::Create(CardGame* outCardGame)
	{
		*outCardGame = {};
		{
			EngineCreateInfo engineCreateInfo{};
			outCardGame->_engine = Engine::Create(engineCreateInfo);
		}

		outCardGame->_arena = outCardGame->_engine.CreateSubArena(1024);
		outCardGame->_scene = jv::ge::CreateScene();

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
			outCardGame->_renderTasks->Allocate(outCardGame->_arena, 512);
			outCardGame->_dynamicRenderTasks = &outCardGame->_engine.AddTaskSystem<DynamicRenderTask>();
			outCardGame->_dynamicRenderTasks->Allocate(outCardGame->_arena, 32);
			outCardGame->_textTasks = &outCardGame->_engine.AddTaskSystem<TextTask>();
			outCardGame->_textTasks->Allocate(outCardGame->_arena, 16);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = jv::ge::GetResolution();

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = outCardGame->_scene;
			enableInfo.capacity = 512;

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

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.instancedRenderTasks = outCardGame->_renderTasks;
			textInterpreterCreateInfo.alphabetSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.symbolSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberSubTexture = outCardGame->_subTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
			outCardGame->_textInterpreter = &outCardGame->_engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*outCardGame->_textTasks, textInterpreterCreateInfo);
		}
	}

	void CardGame::Destroy(const CardGame& cardGame)
	{
		jv::Arena::Destroy(cardGame._arena);
		Engine::Destroy(cardGame._engine);
	}

	void CardGame::LoadMainMenu()
	{
		const auto ptr = _arena.New<MainMenuState>();
		_levelState = ptr;
		ptr->saveDataValid = TryLoadSaveData(_playerState);
	}

	void CardGame::UpdateMainMenu()
	{
		const auto ptr = static_cast<MainMenuState*>(_levelState);
		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = -.25f;
		buttonRenderTask.scale.y *= .2f;
		buttonRenderTask.subTexture = _subTextures[static_cast<uint32_t>(TextureId::fallback)];
		_renderTasks->Push(buttonRenderTask);

		TextTask buttonTextTask{};
		buttonTextTask.lineLength = 6;
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "new game";
		_textTasks->Push(buttonTextTask);

		buttonRenderTask.position.y *= -1;
		_renderTasks->Push(buttonRenderTask);
		
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "continue";
		_textTasks->Push(buttonTextTask);
	}
}
