#pragma once

namespace game
{
	struct InputState final
	{
		enum State
		{
			idle,
			pressed,
			released
		};

		glm::ivec2 mousePos{};
		State lMouse{};
		State rMouse{};
		float scroll = 0;
		State enter{};
	};
}