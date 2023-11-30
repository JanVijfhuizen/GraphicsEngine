﻿#pragma once
#include "Level.h"

namespace game
{
	struct MainMenuLevel final : Level
	{
		bool saveDataValid;
		bool inTutorial;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
