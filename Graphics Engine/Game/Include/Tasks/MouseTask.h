#pragma once
#include "States/InputState.h"

namespace game
{
	struct MouseTask final
	{
		glm::vec2 position;
		InputState::State lButton = InputState::idle;
		InputState::State rButton = InputState::idle;
		float scroll = 0;
	};
}
