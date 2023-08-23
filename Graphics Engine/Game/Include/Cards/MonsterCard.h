#pragma once
#include "Card.h"

namespace game
{
	struct MonsterCard final : Card
	{
		uint32_t attack = 1;
		uint32_t armorClass = 4;
		uint32_t health = 2;
	};
}
