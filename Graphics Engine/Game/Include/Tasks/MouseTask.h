#pragma once

namespace game
{
	struct MouseTask final
	{
		glm::vec2 position;

		enum State
		{
			idle,
			pressed,
			released
		};
		State lButton = idle;
		State rButton = idle;
		float scroll = 0;
	};
}