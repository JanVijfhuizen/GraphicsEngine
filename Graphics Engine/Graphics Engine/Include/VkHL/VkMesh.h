#pragma once
#include "VkBuffer.h"
#include "JLib/Array.h"

namespace jv::vk
{
	struct App;
	struct FreeArena;
	typedef uint16_t VertexIndex;

	// Model used for rendering.
	struct Mesh final
	{
		Array<Buffer> vertexBuffers;
		Buffer indexBuffer;
		uint32_t indexCount;

		void Draw(Arena& tempArena, VkCommandBuffer cmd, uint32_t count) const;
		static Mesh Create(Arena& arena, const FreeArena& freeArena, const App& app,
			void** vertices, const uint32_t* verticesSizes, uint32_t vertexCount, const Array<VertexIndex>& indices);
		static void Destroy(Arena& arena, const FreeArena& freeArena, const App& app, const Mesh& mesh, bool freeArenaMemory);
	};
}
