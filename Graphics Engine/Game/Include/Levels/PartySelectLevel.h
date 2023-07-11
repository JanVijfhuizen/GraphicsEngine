#pragma once
#include "Level.h"

namespace game
{
	struct PartySelectLevel final : Level
	{
		bool selected[PARTY_CAPACITY];

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
