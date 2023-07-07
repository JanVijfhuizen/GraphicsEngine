#pragma once
#include "Level.h"

namespace game
{
	struct NewGameLevel final : Level
	{
		jv::Array<uint32_t> monsterDiscoverOptions;
		jv::Array<uint32_t> artifactDiscoverOptions;

		uint32_t monsterChoice = -1;
		uint32_t artifactChoice = -1;
		bool confirmedChoices = false;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
