#pragma once
#include "Cards/Card.h"

namespace game
{
	struct LevelInfo;

	struct CombatStats final
	{
		uint32_t attack;
		uint32_t health;
		uint32_t tempAttack = 0;
		uint32_t tempHealth = 0;
	};

	struct BoardState final
	{
		uint32_t ids[BOARD_CAPACITY]{};
		uint32_t uniqueIds[BOARD_CAPACITY];
		CombatStats combatStats[BOARD_CAPACITY];
		uint32_t allyCount;
		uint32_t enemyCount;

		[[nodiscard]] bool Validate(const ActionState& actionState, const bool src, const bool dst) const
		{
			bool valid = true;
			if (src)
				valid = actionState.source == ActionState::Source::other ? true : uniqueIds[actionState.src] == actionState.srcUniqueId;
			if (dst)
				valid = valid ? uniqueIds[actionState.dst] == actionState.dstUniqueId : false;
			return valid;
		}
	};
}