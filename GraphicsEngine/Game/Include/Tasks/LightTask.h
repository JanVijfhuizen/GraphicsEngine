#pragma once

namespace game
{
	struct LightTask final
	{
		glm::vec4 color{1};
		glm::vec4 pos{0.f, 0.f, 1.f, 0};
		float intensity = 1;
		float fallOf = 2;
		float size = .1f;
		float specularity = 10;

		[[nodiscard]] static glm::vec4 ToLightTaskPos(const glm::ivec2 pos)
		{
			auto ret = glm::vec4(glm::vec2(pos) / glm::vec2(SIMULATED_RESOLUTION), 0, 0);
			ret *= 2;
			ret -= 1;
			ret.y *= -1;
			return ret;
		}
	};
}