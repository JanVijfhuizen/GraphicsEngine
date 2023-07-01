#pragma once

namespace game
{
	struct BoardState final
	{
		uint32_t monsterIds[BOARD_CAPACITY]{};
		uint32_t healths[BOARD_CAPACITY]{};
		uint8_t enemyTargetDices[BOARD_CAPACITY_PER_SIDE]{};
		uint32_t alliedMonsterCount = 0;
		uint32_t enemyMonsterCount = 0;
	};
}