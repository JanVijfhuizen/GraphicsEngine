#pragma once
#include "JLib/Array.h"
#include "Vk/VkImage.h"

namespace jv::vk
{
	struct App;
	struct SubTexture;

	// Generate a single texture from multiple smaller ones.
	void GenerateTextureAtlas(Arena& arena, Arena& tempArena, const Array<const char*>& filePaths, const char* imageFilePath, const char* metaFilePath, uint32_t texChannels = 4);
	[[nodiscard]] Image LoadTexture(Arena& arena, const FreeArena& freeArena, const App& app, const ImageCreateInfo& info, const char* imageFilePath);
	// Load coordinates that correspond with a texture atlas.
	[[nodiscard]] Array<SubTexture> LoadTextureAtlasMetaData(Arena& arena, const char* metaFilePath);
}
