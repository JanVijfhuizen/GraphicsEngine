#pragma once
#include "GE/SubTexture.h"

namespace game
{
	struct PixelPerfectRenderTask final
	{
		glm::ivec2 position{};
		glm::ivec2 scale{ 32 };
		glm::vec4 color{ 1 };
		jv::ge::SubTexture subTexture{};
		bool priority = false;
	};
}
