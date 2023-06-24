#pragma once
#include "VkHL/VkSubTexture.h"

namespace game
{
	struct InstancedRenderTask final
	{
		glm::vec2 position{};
		glm::vec2 scale{ 1 };
		jv::vk::SubTexture subTexture{};
		glm::vec4 color{ 1 };
	};
}
