#include "pch.h"
#include "VkHL/VkShapes.h"

#include "JLib/ArrayUtils.h"

namespace jv::vk
{
	uint64_t CreateQuadShape(Arena& arena, Array<Vertex2d>& outVertices, Array<VertexIndex>& outIndices, const float scale)
	{
		auto scope = arena.CreateScope();

		outVertices = CreateArray<Vertex2d>(arena, 4);
		outIndices = CreateArray<VertexIndex>(arena, 6);

		outVertices[0].position = { -1, -1 };
		outVertices[1].position = { -1, 1 };
		outVertices[2].position = { 1, 1 };
		outVertices[3].position = { 1, -1 };

		outVertices[0].textureCoordinates = { 0, 0 };
		outVertices[1].textureCoordinates = { 0, 1 };
		outVertices[2].textureCoordinates = { 1, 1 };
		outVertices[3].textureCoordinates = { 1, 0 };

		outIndices[0] = 0;
		outIndices[1] = 1;
		outIndices[2] = 2;
		outIndices[3] = 0;
		outIndices[4] = 2;
		outIndices[5] = 3;

		for (auto& vertex : outVertices)
			vertex.position *= scale;

		return scope;
	}
}
