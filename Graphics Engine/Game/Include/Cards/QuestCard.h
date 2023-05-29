#pragma once
#include "Dice.h"

namespace game
{
	struct QuestCard final : Card
	{
		uint32_t tier;
		Dice roomCountDice;
		uint32_t minRoomCount;
	};
}
