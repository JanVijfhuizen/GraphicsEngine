#include "pch_game.h"

#include <stb_image.h>

#include "Interpreters/InstancedRenderInterpreter.h"
#include "Engine/Engine.h"
#include "Interpreters/TextInterpreter.h"
#include "GE/AtlasGenerator.h"

struct TextureLibrary final
{
	enum Indexes
	{
		alphabet,
		numbers,
		symbols,
		fallback
	};

	const char* atlasPath = "Art/Atlas.png";
	const char* atlasMetaDataPath = "Art/AtlasMetaData.txt";
	jv::ge::Resource atlas;
	jv::Array<jv::ge::SubTexture> subTextures;
};

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	const auto scene = jv::ge::CreateScene();
	auto subArena = engine.CreateSubArena(1024);

	TextureLibrary textureLibrary{};

#ifdef _DEBUG
	{
		const char* texturePathValues[]
		{
			"Art/alphabet.png",
			"Art/numbers.png",
			"Art/symbols.png",
			"Art/fallback.png"
		};

		jv::Array<const char*> texturePaths{};
		texturePaths.ptr = texturePathValues;
		texturePaths.length = sizeof(texturePathValues) / sizeof(const char*);

		jv::ge::GenerateAtlas(subArena, engine.GetMemory().tempArena,texturePaths,
		                      textureLibrary.atlasPath, textureLibrary.atlasMetaDataPath);
	}
#endif

	int texWidth, texHeight, texChannels2;
	stbi_uc* pixels = stbi_load(textureLibrary.atlasPath, &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

	jv::ge::ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.resolution = { texWidth, texHeight };
	imageCreateInfo.scene = scene;
	textureLibrary.atlas = AddImage(imageCreateInfo);
	jv::ge::FillImage(textureLibrary.atlas, pixels);

	textureLibrary.subTextures = jv::ge::LoadAtlasMetaData(subArena, textureLibrary.atlasMetaDataPath);

	auto& renderTasks = engine.AddTaskSystem<game::InstancedRenderTask>();
	renderTasks.Allocate(subArena, 32);

	auto& textTasks = engine.AddTaskSystem<game::TextTask>();
	textTasks.Allocate(subArena, 16);

	game::InstancedRenderInterpreterCreateInfo createInfo{};
	createInfo.resolution = jv::ge::GetResolution();

	game::InstancedRenderInterpreterEnableInfo enableInfo{};
	enableInfo.scene = scene;
	enableInfo.capacity = 32;

	auto& renderInterpreter = engine.AddTaskInterpreter<game::InstancedRenderTask, game::InstancedRenderInterpreter>(
		renderTasks, createInfo);
	renderInterpreter.Enable(enableInfo);
	renderInterpreter.image = textureLibrary.atlas;

	game::TextInterpreterCreateInfo textInterpreterCreateInfo{};
	textInterpreterCreateInfo.instancedRenderTasks = &renderTasks;
	textInterpreterCreateInfo.alphabetSubTexture = textureLibrary.subTextures[TextureLibrary::alphabet];
	textInterpreterCreateInfo.symbolSubTexture = textureLibrary.subTextures[TextureLibrary::symbols];
	textInterpreterCreateInfo.numberSubTexture = textureLibrary.subTextures[TextureLibrary::numbers];
	textInterpreterCreateInfo.atlasResolution = glm::ivec2(texWidth, texHeight);
	auto& textInterpreter = engine.AddTaskInterpreter<game::TextTask, game::TextInterpreter>(
		textTasks, textInterpreterCreateInfo);

	while(true)
	{
		static float f = 0;
		f += 0.01f;
		
		constexpr float dist = .2f;

		for (uint32_t i = 0; i < 5; ++i)
		{
			game::InstancedRenderTask renderTask{};
			renderTask.position.x = sin(f + dist * i) * .25f;
			renderTask.position.y = cos(f + dist * i) * .25f;
			renderTask.position *= 1.f + (5.f - static_cast<float>(i)) * .2f;
			renderTask.color.r *= static_cast<float>(i) / 5;
			renderTask.scale *= .2f;
			renderTask.subTexture = textureLibrary.subTextures[TextureLibrary::fallback];
			renderTasks.Push(renderTask);
		}

		game::TextTask textTask{};
		textTask.text = "just a test.";
		textTask.position.x = -1;
		textTask.position.y = -.75f;
		textTasks.Push(textTask);

		renderInterpreter.camera.position.x = abs(sin(f / 3)) * .2f;
		renderInterpreter.camera.zoom = -.4f + abs(cos(f / 2)) * .2f;
		renderInterpreter.camera.rotation = sin(f / 4) * .2f;

		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
