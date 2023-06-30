#pragma once

namespace game
{
	struct MouseTask final
	{
		glm::vec2 position;
		bool lPressed = false;
		bool rPressed = false;
		bool mPressed = false;
	};
}