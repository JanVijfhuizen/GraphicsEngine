#pragma once
#include "Level.h"

namespace game
{
	struct MainLevel final : Level
	{
		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
