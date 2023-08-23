#pragma once
#include "Card.h"

namespace game
{
	struct MagicCard final : Card
	{
		enum class Type
		{
			target,
			all
		} type = Type::target;
		uint32_t cost = 1;
	};
}
