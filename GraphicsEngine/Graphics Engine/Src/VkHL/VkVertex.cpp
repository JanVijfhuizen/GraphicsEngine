#include "pch.h"
#include "VkHL/VkVertex.h"

#include "JLib/ArrayUtils.h"

namespace jv::vk
{
	Array<VkVertexInputBindingDescription> GetBindingDescriptionsV2DPoint(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputBindingDescription>(arena, 1);
		auto& vertexData = ret[0];
		vertexData.binding = 0;
		vertexData.stride = sizeof(Vertex2dPoint);
		vertexData.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return ret;
	}

	Array<VkVertexInputAttributeDescription> GetAttributeDescriptionsV2DPoint(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputAttributeDescription>(arena, 1);
		auto& position = ret[0];
		position.binding = 0;
		position.location = 0;
		position.format = VK_FORMAT_R32G32_SFLOAT;
		position.offset = 0;
		return ret;
	}

	Array<VkVertexInputBindingDescription> GetBindingDescriptionsV3DPoint(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputBindingDescription>(arena, 1);
		auto& vertexData = ret[0];
		vertexData.binding = 0;
		vertexData.stride = sizeof(Vertex3dPoint);
		vertexData.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return ret;
	}

	Array<VkVertexInputAttributeDescription> GetAttributeDescriptionsV3DPoint(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputAttributeDescription>(arena, 1);
		auto& position = ret[0];
		position.binding = 0;
		position.location = 0;
		position.format = VK_FORMAT_R32G32B32_SFLOAT;
		position.offset = 0;
		return ret;
	}

	Array<VkVertexInputBindingDescription> Vertex2d::GetBindingDescriptions(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputBindingDescription>(arena, 1);
		auto& vertexData = ret[0];
		vertexData.binding = 0;
		vertexData.stride = sizeof(Vertex2d);
		vertexData.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return ret;
	}

	Array<VkVertexInputAttributeDescription> Vertex2d::GetAttributeDescriptions(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputAttributeDescription>(arena, 2);

		auto& position = ret[0];
		position.binding = 0;
		position.location = 0;
		position.format = VK_FORMAT_R32G32_SFLOAT;
		position.offset = offsetof(Vertex2d, position);

		auto& texCoords = ret[1];
		texCoords.binding = 0;
		texCoords.location = 1;
		texCoords.format = VK_FORMAT_R32G32B32_SFLOAT;
		texCoords.offset = offsetof(Vertex2d, textureCoordinates);

		return ret;
	}

	Array<VkVertexInputBindingDescription> Vertex3d::GetBindingDescriptions(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputBindingDescription>(arena, 1);
		auto& vertexData = ret[0];
		vertexData.binding = 0;
		vertexData.stride = sizeof(Vertex3d);
		vertexData.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return ret;
	}

	Array<VkVertexInputAttributeDescription> Vertex3d::GetAttributeDescriptions(Arena& arena)
	{
		const auto ret = CreateArray<VkVertexInputAttributeDescription>(arena, 3);

		auto& position = ret[0];
		position.binding = 0;
		position.location = 0;
		position.format = VK_FORMAT_R32G32B32_SFLOAT;
		position.offset = offsetof(Vertex3d, position);

		auto& normal = ret[1];
		normal.binding = 0;
		normal.location = 1;
		normal.format = VK_FORMAT_R32G32B32_SFLOAT;
		normal.offset = offsetof(Vertex3d, normal);

		auto& texCoords = ret[2];
		texCoords.binding = 0;
		texCoords.location = 2;
		texCoords.format = VK_FORMAT_R32G32_SFLOAT;
		texCoords.offset = offsetof(Vertex3d, textureCoordinates);

		return ret;
	}
}
