#pragma once
#include "VkVertex.h"
#include "JLib/Array.h"

namespace jv::vk
{
	void CreateQuadShape(Arena& arena, Array<Vertex2d>& outVertices, Array<VertexIndex>& outIndices, float scale = 1);
}
