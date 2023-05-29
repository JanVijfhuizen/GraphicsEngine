#pragma once

namespace game
{
	struct MonsterCard final : Card
	{
		uint32_t tier;
		bool unique = false;
	};
}