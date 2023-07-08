#pragma once
#include "Level.h"

namespace game
{
	struct MainLevel final : Level
	{
		enum class Stage
		{
			bossReveal,
			roomSelection
		} stage;
		bool switchingStage;

		struct Boss final
		{
			uint32_t id;
			uint32_t counters;
		};

		uint32_t depth;
		jv::Array<Boss> currentBosses;
		jv::Vector<uint32_t> bossDeck;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;

		void SwitchToBossRevealStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		void UpdateBossRevealStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		void SwitchToRoomSelectionStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		void UpdateRoomSelectionStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
	};
}
