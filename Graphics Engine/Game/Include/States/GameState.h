#pragma once

namespace game
{
	struct GameState final
	{
		uint32_t partyMembers[PARTY_ACTIVE_CAPACITY]{};
		uint32_t flaws[PARTY_ACTIVE_CAPACITY * MONSTER_FLAW_CAPACITY]{};
		uint32_t flawsCount[PARTY_ACTIVE_CAPACITY]{};
		uint32_t magics[MAGIC_CAPACITY]{};
		uint32_t partySize = 0;
	};
}