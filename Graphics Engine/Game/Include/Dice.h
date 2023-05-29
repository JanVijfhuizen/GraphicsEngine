#pragma once

namespace game
{
	struct Dice final
	{
		enum class Type
		{
			d4,
			d6,
			d20
		} dType = Type::d20;
		uint32_t count = 1;
	};
}