#pragma once
#include "GE/SubTexture.h"

namespace game
{
	struct InstancedRenderTask final
	{
		glm::vec2 position{};
		glm::vec2 scale{ 1 };
		jv::ge::SubTexture subTexture{};
		glm::vec4 color{ 1 };
	};
}
