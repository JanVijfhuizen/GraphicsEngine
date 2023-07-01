#pragma once

namespace game
{
	struct GameState final
	{
		uint8_t chosenPartyMembers[PARTY_ACTIVE_CAPACITY]{};
		uint32_t flaws[PARTY_ACTIVE_CAPACITY * MONSTER_FLAW_CAPACITY]{};
		uint8_t flawsCount[PARTY_ACTIVE_CAPACITY]{};
		uint32_t magicDeck[MAGIC_DECK_SIZE]{};
		uint32_t partySize = 0;
	};
}