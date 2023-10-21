#pragma once
#include "Card.h"

namespace game
{
	struct EventCard final : Card
	{
		[[nodiscard]] Type GetType() override
		{
			return Type::event;
		}
	};
}
