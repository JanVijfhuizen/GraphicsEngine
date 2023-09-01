#pragma once
#include "Level.h"

namespace game
{
	struct GameOverLevel final : Level
	{
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
