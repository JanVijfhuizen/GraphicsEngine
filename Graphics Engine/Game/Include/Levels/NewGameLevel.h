#pragma once
#include "Level.h"
#include "LevelStates/LevelStateMachine.h"

namespace game
{
	struct NewGameLevel final : Level
	{
		struct State final
		{
			uint32_t monsterId;
			uint32_t artifactId;
		};

		struct ModeSelectState final : LevelState<State>
		{
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		struct PartySelectState final : LevelState<State>
		{
			jv::Vector<uint32_t> monsterDeck;
			jv::Vector<uint32_t> artifactDeck;
			jv::Array<uint32_t> monsterDiscoverOptions;
			jv::Array<uint32_t> artifactDiscoverOptions;
			uint32_t monsterChoice;
			uint32_t artifactChoice;
			float timeSinceFirstChoicesMade;

			bool Create(State& state, const LevelCreateInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		struct JoinState final : LevelState<State>
		{
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		LevelStateMachine<State> stateMachine;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
