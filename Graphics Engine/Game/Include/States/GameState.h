#pragma once

namespace game
{
	struct Library;
	struct PlayerState;

	struct GameState final
	{
		PlayerState* playerState;
		Library* library;
	};
}