#pragma once
#include <Macros.h>

namespace game
{
	struct MonsterState final
	{
		uint32_t damageTaken;
	};

	struct BoardState final
	{
		MonsterState monsterStates[MAX_PARTY_SIZE + MAX_TIER];
		uint32_t monsterIntentions[MAX_TIER];
		uint32_t monsterIds[MAX_TIER];
		uint32_t monsterCount;
	};
}