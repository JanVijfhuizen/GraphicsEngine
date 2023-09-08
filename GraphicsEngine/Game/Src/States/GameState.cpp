#include "pch_game.h"
#include "States/GameState.h"

namespace game
{
	GameState GameState::Create()
	{
		GameState gameState{};
		for (auto& flaw : gameState.flaws)
			flaw = -1;
		return gameState;
	}
}
