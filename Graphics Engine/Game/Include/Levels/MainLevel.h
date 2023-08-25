#pragma once
#include "Level.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/BoardState.h"

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

		struct State final
		{
			struct Decks final
			{
				jv::Vector<uint32_t> monsters;
				jv::Vector<uint32_t> artifacts;
				jv::Vector<uint32_t> bosses;
				jv::Vector<uint32_t> rooms;
				jv::Vector<uint32_t> magics;
				jv::Vector<uint32_t> flaws;
				jv::Vector<uint32_t> events;

				[[nodiscard]] static Decks Create(const LevelCreateInfo& info);
			} decks{};

			struct Path final
			{
				uint32_t boss = UINT32_MAX;
				uint32_t room = UINT32_MAX;
				uint32_t magic = UINT32_MAX;
				uint32_t artifact = UINT32_MAX;
				uint32_t flaw = UINT32_MAX;
				uint32_t counters = 0;
			};

			uint32_t depth = 0;
			jv::Array<Path> paths;
			uint32_t chosenPath;

			jv::Vector<uint32_t> magicDeck;
			jv::Vector<uint32_t> hand;
			BoardState boardState;

			void RemoveDuplicates(const LevelInfo& info, jv::Vector<uint32_t>& deck, uint32_t Path::* mem) const;
			[[nodiscard]] uint32_t GetMonster(const LevelInfo& info);
			[[nodiscard]] uint32_t GetBoss(const LevelInfo& info);
			[[nodiscard]] uint32_t GetRoom(const LevelInfo& info);
			[[nodiscard]] uint32_t GetMagic(const LevelInfo& info);
			[[nodiscard]] uint32_t GetArtifact(const LevelInfo& info);
			[[nodiscard]] uint32_t GetFlaw(const LevelInfo& info);
			[[nodiscard]] uint32_t GetEvent(const LevelInfo& info);
			[[nodiscard]] uint32_t Draw(const LevelInfo& info);
			[[nodiscard]] uint32_t GetPrimaryPath() const;

			[[nodiscard]] static State Create(const LevelCreateInfo& info);
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
			enum class TurnState
			{
				startOfTurn,
				playerTurn,
				combat
			} turnState;
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
			uint32_t lastEnemyDefeatedId;

			void Reset(State& state, const LevelInfo& info) override;
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;

			static void Attack(State& state, uint32_t src, uint32_t dst);
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
			bool Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
				LevelIndex& loadLevelIndex) override;
		};

		LevelStateMachine<State> stateMachine;
		
		void Create(const LevelCreateInfo& info) override;
		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) override;
	};
}
