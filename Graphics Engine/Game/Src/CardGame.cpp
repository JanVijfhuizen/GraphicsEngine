#include "pch_game.h"
#include "CardGame.h"

#include <stb_image.h>
#include <Engine/Engine.h>

#include "GE/AtlasGenerator.h"
#include "GE/GraphicsEngine.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/ArrayUtils.h"

namespace game 
{
	constexpr const char* ATLAS_PATH = "Art/Atlas.png";
	constexpr const char* ATLAS_META_DATA_PATH = "Art/AtlasMetaData.txt";

	jv::Array<const char*> GetTexturePaths(jv::Arena& arena)
	{
		const auto arr = jv::CreateArray<const char*>(arena, static_cast<uint32_t>(CardGame::TextureId::length));
		arr[0] = "Art/alphabet.png";
		arr[1] = "Art/numbers.png";
		arr[2] = "Art/symbols.png";
		arr[3] = "Art/fallback.png";
		return arr;
	}

	bool CardGame::Update()
	{
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

		const bool result = _engine.Update();
		return result;
	}

	CardGame CardGame::Create()
	{
		CardGame cardGame{};

		{
			EngineCreateInfo engineCreateInfo{};
			cardGame._engine = Engine::Create(engineCreateInfo);
		}

		cardGame._arena = cardGame._engine.CreateSubArena(1024);
		cardGame._scene = jv::ge::CreateScene();

		const auto mem = cardGame._engine.GetMemory();

#ifdef _DEBUG
		{
			const auto tempScope = mem.tempArena.CreateScope();

			const auto paths = GetTexturePaths(mem.tempArena);
			jv::ge::GenerateAtlas(cardGame._arena, mem.tempArena, paths,
				ATLAS_PATH, ATLAS_META_DATA_PATH);

			mem.tempArena.DestroyScope(tempScope);
		}
#endif

		int texWidth, texHeight, texChannels2;
		{
			stbi_uc* pixels = stbi_load(ATLAS_PATH, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

			jv::ge::ImageCreateInfo imageCreateInfo{};
			imageCreateInfo.resolution = { texWidth, texHeight };
			imageCreateInfo.scene = cardGame._scene;
			cardGame._atlas = AddImage(imageCreateInfo);
			jv::ge::FillImage(cardGame._atlas, pixels);
			stbi_image_free(pixels);
			cardGame._subTextures = jv::ge::LoadAtlasMetaData(cardGame._arena, ATLAS_META_DATA_PATH);
		}

		{
			cardGame._renderTasks = &cardGame._engine.AddTaskSystem<RenderTask>();
			cardGame._renderTasks->Allocate(cardGame._arena, 32);
			cardGame._dynamicRenderTasks = &cardGame._engine.AddTaskSystem<DynamicRenderTask>();
			cardGame._dynamicRenderTasks->Allocate(cardGame._arena, 32);
			cardGame._textTasks = &cardGame._engine.AddTaskSystem<TextTask>();
			cardGame._textTasks->Allocate(cardGame._arena, 16);
		}

		{
			InstancedRenderInterpreterCreateInfo createInfo{};
			createInfo.resolution = jv::ge::GetResolution();

			InstancedRenderInterpreterEnableInfo enableInfo{};
			enableInfo.scene = cardGame._scene;
			enableInfo.capacity = 32;

			cardGame._renderInterpreter = &cardGame._engine.AddTaskInterpreter<RenderTask, InstancedRenderInterpreter>(
				*cardGame._renderTasks, createInfo);
			cardGame._renderInterpreter->Enable(enableInfo);
			cardGame._renderInterpreter->image = cardGame._atlas;

			DynamicRenderInterpreterCreateInfo dynamicCreateInfo{};
			dynamicCreateInfo.resolution = jv::ge::GetResolution();
			dynamicCreateInfo.frameArena = &mem.frameArena;

			DynamicRenderInterpreterEnableInfo dynamicEnableInfo{};
			dynamicEnableInfo.arena = &cardGame._arena;
			dynamicEnableInfo.scene = cardGame._scene;
			dynamicEnableInfo.capacity = 32;

			cardGame._dynamicRenderInterpreter = &cardGame._engine.AddTaskInterpreter<DynamicRenderTask, DynamicRenderInterpreter>(
				*cardGame._dynamicRenderTasks, dynamicCreateInfo);
			cardGame._dynamicRenderInterpreter->Enable(dynamicEnableInfo);

			TextInterpreterCreateInfo textInterpreterCreateInfo{};
			textInterpreterCreateInfo.instancedRenderTasks = cardGame._renderTasks;
			textInterpreterCreateInfo.alphabetSubTexture = cardGame._subTextures[static_cast<uint32_t>(TextureId::alphabet)];
			textInterpreterCreateInfo.symbolSubTexture = cardGame._subTextures[static_cast<uint32_t>(TextureId::symbols)];
			textInterpreterCreateInfo.numberSubTexture = cardGame._subTextures[static_cast<uint32_t>(TextureId::numbers)];
			textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
			cardGame._textInterpreter = &cardGame._engine.AddTaskInterpreter<TextTask, TextInterpreter>(
				*cardGame._textTasks, textInterpreterCreateInfo);
		}

		// Temp until stupid bug is fixed.
		while (true)
			cardGame.Update();

		return cardGame;
	}

	void CardGame::Destroy(const CardGame& cardGame)
	{
		jv::Arena::Destroy(cardGame._arena);
		Engine::Destroy(cardGame._engine);
	}
}
