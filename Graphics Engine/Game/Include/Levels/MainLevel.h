#pragma once
#include "Level.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/BoardState.h"
#include "States/CombatState.h"

namespace game
{
	struct MainLevel final : Level
	{
		enum class StateNames
		{
			bossReveal,
			pathSelect,
			combat,
			rewardMagic,
			rewardFlaw,
			rewardArtifact,
			exitFound
		};

		struct BossRevealState final : LevelState<State>
		{
			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct PathSelectState final : LevelState<State>
		{
			uint32_t discoverOption;
			float timeSinceDiscovered;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct CombatState final : LevelState<State>
		{
			enum class SelectionState
			{
				none,
				ally,
				enemy,
				hand
			} selectionState;

			float time;
			uint32_t eventCard;
			uint32_t targets[BOARD_CAPACITY_PER_SIDE];
			bool tapped[BOARD_CAPACITY_PER_SIDE];
			uint32_t selectedId;
			uint32_t mana;
			uint32_t maxMana;
			uint32_t lastEnemyDefeatedId;
			uint32_t uniqueId;
			float actionEventLifetime;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
			[[nodiscard]] bool HandleActionState(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex, ActionState& actionState);
		};

		struct RewardMagicCardState final : LevelState<State>
		{
			uint32_t discoverOption;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct RewardFlawCardState final : LevelState<State>
		{
			uint32_t discoverOption;
			float timeSinceDiscovered;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct RewardArtifactState final : LevelState<State>
		{
			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct ExitFoundState final : LevelState<State>
		{
			bool managingParty;
			bool selected[PARTY_CAPACITY];
			float timeSincePartySelected;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		LevelStateMachine<State> stateMachine;
		
		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
