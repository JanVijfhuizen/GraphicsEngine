#pragma once
#include "VkVertex.h"
#include "JLib/Array.h"

namespace jv::vk
{
	// Returns a scope.
	[[nodiscard]] uint64_t CreateQuadShape(Arena& arena, Array<Vertex2d>& outVertices, Array<VertexIndex>& outIndices, float scale = 1);
}
