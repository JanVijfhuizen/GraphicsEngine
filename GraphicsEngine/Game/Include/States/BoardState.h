#pragma once

namespace game
{
	struct LevelInfo;

	struct CombatStats final
	{
		uint32_t attack;
		uint32_t health;
		uint32_t armorClass;
	};

	struct CombatStatModifier final
	{
		int32_t attack = 0;
		int32_t health = 0;
		int32_t armorClass = 0;

		[[nodiscard]] CombatStats GetProcessedCombatStats(const CombatStats& stats) const
		{
			CombatStats ret = stats;
			ret.attack = attack < 0 && static_cast<uint32_t>(-attack) > stats.attack ? 0 : stats.attack + attack;
			ret.health = health < 0 && static_cast<uint32_t>(-health) > stats.health ? 0 : stats.health + health;
			ret.armorClass = armorClass < 0 && static_cast<uint32_t>(-armorClass) > stats.armorClass ? 0 : stats.armorClass + armorClass;
			return ret;
		}
	};

	struct BoardState final
	{
		uint32_t ids[BOARD_CAPACITY]{};
		uint32_t partyIds[BOARD_CAPACITY_PER_SIDE];
		uint32_t uniqueIds[BOARD_CAPACITY];
		CombatStats combatStats[BOARD_CAPACITY];
		CombatStatModifier combatStatModifiers[BOARD_CAPACITY]{};
		uint32_t allyCount;
		uint32_t enemyCount;
	};
}