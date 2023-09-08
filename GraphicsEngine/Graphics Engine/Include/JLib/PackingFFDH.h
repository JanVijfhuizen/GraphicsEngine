#pragma once

#include "Array.h"

namespace jv
{
	[[nodiscard]] Array<glm::ivec2> Pack(Arena& arena, Arena& tempArena, const Array<glm::ivec2>& shapes, glm::ivec2& outArea);
}