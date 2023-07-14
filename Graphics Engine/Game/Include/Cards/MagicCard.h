#pragma once
#include "Card.h"

namespace game
{
	struct MagicCard final : Card
	{
		uint32_t cost = 1;
	};
}
