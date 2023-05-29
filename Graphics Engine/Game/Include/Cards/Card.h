#pragma once

namespace game
{
	struct GameState;

	struct Card
	{
		void(*onGameStart)(GameState* state, bool isInParty) = nullptr;
	};
}