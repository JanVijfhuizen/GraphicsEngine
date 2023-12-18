#pragma once

namespace game
{
	struct GameState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t curses[PARTY_CAPACITY]{};
		uint32_t healths[PARTY_CAPACITY]{};
		uint32_t spells[SPELL_DECK_SIZE]{};
		uint32_t partySize;
		uint32_t artifacts[PARTY_CAPACITY * MONSTER_ARTIFACT_CAPACITY];
		uint32_t artifactSlotCount;

		[[nodiscard]] static GameState Create();
	};
}