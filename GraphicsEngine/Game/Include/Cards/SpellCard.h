#pragma once
#include "Card.h"

namespace game
{
	struct SpellCard final : Card
	{
		enum class Type
		{
			target,
			all
		} type = Type::target;
		uint32_t cost = 1;

		[[nodiscard]] Card::Type GetType() override
		{
			return Card::Type::spell;
		}
	};
}
