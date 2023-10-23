#pragma once
#include "Card.h"

namespace game
{
	struct RoomCard final : Card
	{
		[[nodiscard]] Type GetType() override
		{
			return Type::room;
		}
	};
}
