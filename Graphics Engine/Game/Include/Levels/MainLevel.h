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
			float hoverDurations[DISCOVER_LENGTH];
			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct PathSelectState final : LevelState<State>
		{
			float hoverDurations[DISCOVER_LENGTH];
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
			float timeSinceLastActionState;
			ActionState* activeState;
			const char* actiontext;
			float actionStateDuration;
			float hoverDurations[BOARD_CAPACITY + HAND_MAX_SIZE + 2];

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) override;
			[[nodiscard]] bool PreHandleActionState(State& state, const LevelUpdateInfo& info, ActionState& actionState);
			[[nodiscard]] bool PostHandleActionState(State& state, const LevelUpdateInfo& info, ActionState& actionState);
			[[nodiscard]] static bool ValidateActionState(const State& state, ActionState& actionState);
			void DrawAttackAnimation(const State& state, const LevelUpdateInfo& info, const Level& level, CardSelectionDrawInfo& drawInfo, bool allied) const;
			void DrawDamageAnimation(const State& state, const LevelUpdateInfo& info, const Level& level, CardSelectionDrawInfo& drawInfo, bool allied) const;
			void DrawSummonAnimation(const State& state, const LevelUpdateInfo& info, const Level& level, CardSelectionDrawInfo& drawInfo, bool allied) const;
			void DrawDrawAnimation(const State& state, const LevelUpdateInfo& info, const Level& level, CardSelectionDrawInfo& drawInfo) const;
		};

		struct RewardMagicCardState final : LevelState<State>
		{
			float hoverDurations[MAGIC_DECK_SIZE + 1];
			uint32_t discoverOption;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct RewardFlawCardState final : LevelState<State>
		{
			float hoverDurations[PARTY_ACTIVE_CAPACITY + 1];
			uint32_t discoverOption;
			float timeSinceDiscovered;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		struct RewardArtifactState final : LevelState<State>
		{
			float hoverDurations[PARTY_ACTIVE_CAPACITY + 1];
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
