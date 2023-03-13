﻿#pragma once
#include "JLib/Array.h"

namespace jv::vk
{
	// Defines a vertex point in a mesh.
	struct Vertex2d final
	{
		typedef uint16_t Index;

		glm::vec2 position{};
		glm::vec2 textureCoordinates{};

		[[nodiscard]] static Array<VkVertexInputBindingDescription> GetBindingDescriptions(Arena& arena);
		[[nodiscard]] static Array<VkVertexInputAttributeDescription> GetAttributeDescriptions(Arena& arena);
	};

	// Defines a vertex point in a mesh.
	struct Vertex3d final
	{
		typedef uint16_t Index;

		glm::vec3 position{};
		// Forward direction of a vertex.
		glm::vec3 normal{ 0, 0, 1 };
		glm::vec2 textureCoordinates{};

		[[nodiscard]] static Array<VkVertexInputBindingDescription> GetBindingDescriptions(Arena& arena);
		[[nodiscard]] static Array<VkVertexInputAttributeDescription> GetAttributeDescriptions(Arena& arena);
	};
}
