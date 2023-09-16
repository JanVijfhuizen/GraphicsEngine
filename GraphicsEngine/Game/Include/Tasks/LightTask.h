#pragma once

namespace game
{
	struct LightTask final
	{
		glm::vec3 color{1};
		float pad0;
		glm::vec3 pos{0.f, 0.f, 1.f};
		float intensity = 1;
		float fallOf = 2;
	};
}