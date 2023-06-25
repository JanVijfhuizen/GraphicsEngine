#pragma once
#include "Dice.h"

namespace game
{
	struct QuestCard final
	{
		uint32_t tier;
		Dice roomCountDice;
		uint32_t minRoomCount;
		uint32_t bossId;
	};
}
