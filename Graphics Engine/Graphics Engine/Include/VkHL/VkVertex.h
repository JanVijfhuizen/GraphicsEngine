#pragma once
#include "JLib/Array.h"

namespace jv::vk
{
	typedef uint16_t VertexIndex;

	// Defines a vertex point in a mesh.
	struct Vertex2d final
	{
		glm::vec2 position{};
		glm::vec2 textureCoordinates{};

		[[nodiscard]] static Array<VkVertexInputBindingDescription> GetBindingDescriptions(Arena& arena);
		[[nodiscard]] static Array<VkVertexInputAttributeDescription> GetAttributeDescriptions(Arena& arena);
	};

	// Defines a vertex point in a mesh.
	struct Vertex3d final
	{
		glm::vec3 position{};
		// Forward direction of a vertex.
		glm::vec3 normal{ 0, 0, 1 };
		glm::vec2 textureCoordinates{};

		[[nodiscard]] static Array<VkVertexInputBindingDescription> GetBindingDescriptions(Arena& arena);
		[[nodiscard]] static Array<VkVertexInputAttributeDescription> GetAttributeDescriptions(Arena& arena);
	};
}
