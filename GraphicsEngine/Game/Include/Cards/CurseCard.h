#pragma once
#include "Card.h"

namespace game
{
	struct CurseCard final : Card
	{
		[[nodiscard]] Type GetType() override
		{
			return Type::curse;
		}
	};
}
