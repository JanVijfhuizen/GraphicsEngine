#pragma once

namespace game
{
	struct PlayerState;

	struct GameState final
	{
		PlayerState* playerState;
		uint32_t partyHealth[MAX_PARTY_SIZE];
	};
}