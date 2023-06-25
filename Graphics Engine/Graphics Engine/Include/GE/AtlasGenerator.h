#pragma once
#include "JLib/Array.h"

namespace jv::ge
{
	using Resource = void*;
	struct SubTexture;

	// Generate a single texture from multiple smaller ones.
	void GenerateAtlas(Arena& arena, Arena& tempArena, const Array<const char*>& filePaths, const char* imageFilePath, const char* metaFilePath);
	// Load coordinates that correspond with a texture atlas.
	[[nodiscard]] Array<SubTexture> LoadAtlasMetaData(Arena& arena, const char* metaFilePath);
}
