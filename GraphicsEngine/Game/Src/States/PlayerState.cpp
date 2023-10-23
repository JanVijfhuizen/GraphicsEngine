#include "pch_game.h"
#include "States/PlayerState.h"

namespace game
{
	PlayerState PlayerState::Create()
	{
		PlayerState state{};
		for (auto& artifact : state.artifacts)
			artifact = -1;
		return state;
	}
}
