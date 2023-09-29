#pragma once

namespace game
{
	struct LevelInfo;

	struct CombatStats final
	{
		uint32_t attack;
		uint32_t health;
	};

	struct BoardState final
	{
		uint32_t ids[BOARD_CAPACITY]{};
		uint32_t partyIds[PARTY_ACTIVE_CAPACITY];
		uint32_t uniqueIds[BOARD_CAPACITY];
		CombatStats combatStats[BOARD_CAPACITY];
		uint32_t allyCount;
		uint32_t enemyCount;
	};
}