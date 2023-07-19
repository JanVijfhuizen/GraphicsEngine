#include "pch_game.h"
#include "States/BoardState.h"
#include "Levels/Level.h"
#include "States/GameState.h"
#include "States/PlayerState.h"

namespace game
{
	void BoardState::AddParty(const LevelInfo& info)
	{
		partyCount = info.gameState.partySize;
		alliedMonsterCount = info.gameState.partySize;
		for (uint32_t i = 0; i < partyCount; ++i)
		{
			partyIds[i] = info.gameState.partyIds[i];
			allyIds[i] = info.playerState.monsterIds[partyIds[i]];
			allyHealths[i] = info.gameState.healths[i];
		}
	}

	bool BoardState::TryAddAlly(const LevelInfo& info, const uint32_t id)
	{
		if (alliedMonsterCount > BOARD_CAPACITY_PER_SIDE)
			return false;
		allyIds[alliedMonsterCount] = id;
		allyHealths[alliedMonsterCount++] = info.monsters[id].health;
		return true;
	}

	void BoardState::RemoveAlly(const uint32_t i)
	{
		for (uint32_t j = i; j < alliedMonsterCount - 1; ++j)
		{
			allyIds[j] = allyIds[j + 1];
			allyHealths[j] = allyHealths[j + 1];

			if (j + 1 < partyCount)
				partyIds[j] = partyIds[j + 1];
		}

		--alliedMonsterCount;
		if (i < partyCount)
			--partyCount;
	}

	bool BoardState::TryAddEnemy(const LevelInfo& info, const uint32_t id)
	{
		if (enemyMonsterCount > BOARD_CAPACITY_PER_SIDE)
			return false;
		enemyIds[enemyMonsterCount] = id;
		enemyHealths[enemyMonsterCount++] = info.monsters[id].health;
		return true;
	}

	void BoardState::RemoveEnemy(const uint32_t i)
	{
		for (uint32_t j = i; j < enemyMonsterCount - 1; ++j)
		{
			enemyIds[j] = enemyIds[j + 1];
			enemyHealths[j] = enemyHealths[j + 1];
			enemyTargets[j] = enemyTargets[j + 1];
		}
		--enemyMonsterCount;
	}

	void BoardState::DealDamage(const uint32_t i, const uint32_t damage)
	{
		if (healths[i] > damage)
		{
			healths[i] -= damage;
			if(i > BOARD_CAPACITY_PER_SIDE)
				RerollEnemyTarget(i - BOARD_CAPACITY_PER_SIDE);
		}
		else
		{
			if (i < BOARD_CAPACITY_PER_SIDE)
				RemoveAlly(i);
			else
				RemoveEnemy(i - BOARD_CAPACITY_PER_SIDE);
		}
	}

	void BoardState::RerollEnemyTarget(const uint32_t i)
	{
		const uint32_t target = rand() % alliedMonsterCount;
		enemyTargets[i] = target;
	}
}
