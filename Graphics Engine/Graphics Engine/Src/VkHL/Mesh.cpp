#include "pch.h"
#include "VkHL/Mesh.h"

namespace jv::vk
{
	void Mesh::Draw(const VkCommandBuffer cmd, const size_t count) const
	{
		constexpr VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);
		vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indexCount), static_cast<uint32_t>(count), 0, 0, 0);
	}

	void Mesh::Destroy(const FreeArena& freeArena, const Mesh& mesh, const App& app)
	{
		freeArena.Free(mesh.indexBuffer.memoryHandle);
		freeArena.Free(mesh.vertexBuffer.memoryHandle);

		vkDestroyBuffer(app.device, mesh.indexBuffer.buffer, nullptr);
		vkDestroyBuffer(app.device, mesh.vertexBuffer.buffer, nullptr);
	}
}
