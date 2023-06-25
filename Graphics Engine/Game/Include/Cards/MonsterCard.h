#pragma once

namespace game
{
	struct MonsterCard final
	{
		uint32_t tier;
		bool unique = false;
		uint32_t health;
		uint32_t attack;
		uint32_t count;
	};
}