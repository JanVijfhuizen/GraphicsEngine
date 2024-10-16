#pragma once
#include "Level.h"

namespace game
{
	struct MainMenuLevel final : Level
	{
		bool saveDataValid;
		bool inTutorial;
		bool inResolutionSelect;
		bool inCredits;
		float openTime;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
		void DrawTitle(const LevelUpdateInfo& info);
	};
}
