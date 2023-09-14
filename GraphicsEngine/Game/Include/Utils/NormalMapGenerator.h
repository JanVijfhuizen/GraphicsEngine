#pragma once
#include "JLib/Array.h"

namespace game
{
	[[nodiscard]] jv::Array<unsigned char> GenerateNormalMap(const unsigned char* heightMap, glm::ivec2 resolution, jv::Arena& arena);
}
