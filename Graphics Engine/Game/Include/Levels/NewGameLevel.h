#pragma once
#include "Level.h"

namespace game
{
	struct NewGameLevel final : Level
	{
		jv::Vector<uint32_t> monsterDeck;
		jv::Vector<uint32_t> artifactDeck;
		jv::Array<uint32_t> monsterDiscoverOptions;
		jv::Array<uint32_t> artifactDiscoverOptions;

		uint32_t monsterChoice;
		uint32_t artifactChoice;
		bool confirmedMode;
		bool confirmedChoices;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
