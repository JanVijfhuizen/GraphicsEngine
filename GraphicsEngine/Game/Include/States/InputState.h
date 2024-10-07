#pragma once

namespace game
{
	struct InputState final
	{
		struct State
		{
			bool pressed = false;
			bool change = false;

			[[nodiscard]] bool ReleaseEvent() const;
			[[nodiscard]] bool PressEvent() const;
		};

		glm::ivec2 mousePos{};
		glm::ivec2 fullScreenMousePos{};
		State lMouse{};
		State rMouse{};
		State esc;
		float scroll = 0;
		State enter{};
	};

	inline bool InputState::State::ReleaseEvent() const
	{
		return !pressed && change;
	}

	inline bool InputState::State::PressEvent() const
	{
		return pressed && change;
	}
}
