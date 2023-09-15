#pragma once

namespace game
{
	struct LightTask final
	{
		glm::vec3 color{1};
		float pad0;
		glm::vec3 pos{.5f, .5f, 1};
		float intensity = 1;
		float fallOf = .5f;
	};
}