#pragma once

namespace game
{
	struct GameState final
	{
		uint32_t partyIds[PARTY_ACTIVE_CAPACITY]{};
		uint32_t monsterIds[PARTY_ACTIVE_CAPACITY]{};
		uint32_t flaws[PARTY_ACTIVE_CAPACITY]{};
		uint32_t healths[PARTY_ACTIVE_CAPACITY]{};
		uint32_t magics[MAGIC_DECK_SIZE]{};
		uint32_t partyCount = 0;
		uint32_t artifacts[PARTY_ACTIVE_CAPACITY * MONSTER_ARTIFACT_CAPACITY];
		uint32_t artifactSlotCounts[PARTY_ACTIVE_CAPACITY]{};

		[[nodiscard]] static GameState Create();
	};
}