#pragma once

namespace game
{
	struct LightTask final
	{
		glm::vec4 color{1};
		glm::vec4 pos{0.f, 0.f, 1.f, 0};
		float intensity = 1;
		float fallOf = 2;
		float pad[2];
	};
}