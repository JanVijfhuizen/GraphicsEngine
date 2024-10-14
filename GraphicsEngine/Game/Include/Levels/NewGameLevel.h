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
		};

		struct PartySelectState final : LevelState<State>
		{
			jv::Vector<uint32_t> monsterDeck;
			jv::Array<uint32_t> monsterDiscoverOptions;
			uint32_t monsterChoice;
			float timeSinceFirstChoicesMade;
			CardDrawMetaData metaDatas[DISCOVER_LENGTH];
			float timeUntilTextBubble;
			uint32_t textBubbleIndex;

			bool Create(State& state, const LevelCreateInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		struct JoinState final : LevelState<State>
		{
			CardDrawMetaData metaData;
			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
		};

		LevelStateMachine<State> stateMachine;

		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
