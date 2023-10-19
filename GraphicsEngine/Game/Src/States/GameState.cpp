#include "pch_game.h"
#include "States/GameState.h"

namespace game
{
	GameState GameState::Create()
	{
		GameState gameState{};
		for (auto& curse : gameState.curses)
			curse = -1;
		return gameState;
	}
}
