#pragma once

namespace game
{
	struct LevelInfo;

	struct BoardState final
	{
		uint32_t ids[BOARD_CAPACITY]{};
		uint32_t allyCount;
		uint32_t enemyCount;
	};
}