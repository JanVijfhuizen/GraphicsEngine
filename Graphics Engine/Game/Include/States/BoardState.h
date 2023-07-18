#pragma once

namespace game
{
	struct LevelInfo;

	struct BoardState final
	{
		uint32_t partyIds[PARTY_ACTIVE_CAPACITY]{};
		uint32_t partyCount = 0;

		union
		{
			uint32_t monsterIds[BOARD_CAPACITY]{};
			struct
			{
				uint32_t allyIds[BOARD_CAPACITY_PER_SIDE];
				uint32_t enemyIds[BOARD_CAPACITY_PER_SIDE];
			};
		};

		union
		{
			uint32_t healths[BOARD_CAPACITY]{};
			struct
			{
				uint32_t allyHealths[BOARD_CAPACITY_PER_SIDE];
				uint32_t enemyHealths[BOARD_CAPACITY_PER_SIDE];
			};
		};

		uint32_t enemyTargets[BOARD_CAPACITY_PER_SIDE];
		uint32_t alliedMonsterCount = 0;
		uint32_t enemyMonsterCount = 0;
		
		void AddParty(const LevelInfo& info);
		bool TryAddAlly(const LevelInfo& info, uint32_t id);
		void RemoveAlly(uint32_t i);
		bool TryAddEnemy(const LevelInfo& info, uint32_t id);
		void RemoveEnemy(uint32_t i);

		void RerollEnemyTarget(uint32_t i);
	};
}