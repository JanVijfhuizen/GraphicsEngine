#pragma once
#include "Level.h"
#include "LevelStates/LevelStateMachine.h"

namespace game
{
	struct NewGameLevel final : Level
	{
		struct StateInfo final
		{
			uint32_t monsterId;
			uint32_t artifactId;
		};

		struct ModeSelectState final : LevelState<StateInfo>
		{
			bool Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		struct PartySelectState final : LevelState<StateInfo>
		{
			jv::Vector<uint32_t> monsterDeck;
			jv::Vector<uint32_t> artifactDeck;
			jv::Array<uint32_t> monsterDiscoverOptions;
			jv::Array<uint32_t> artifactDiscoverOptions;
			uint32_t monsterChoice = -1;
			uint32_t artifactChoice = -1;

			bool Create(StateInfo& state, const LevelCreateInfo& info) override;
			bool Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		struct JoinState final : LevelState<StateInfo>
		{
			bool Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		LevelStateMachine<StateInfo> stateMachine;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
