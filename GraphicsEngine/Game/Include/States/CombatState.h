#pragma once
#include "BoardState.h"
#include "CombatState.h"
#include "Cards/Card.h"
#include "JLib/Array.h"
#include "JLib/Vector.h"

namespace game
{
	struct LevelInfo;
	struct LevelCreateInfo;

	struct State final
	{
		struct Decks final
		{
			jv::Vector<uint32_t> monsters;
			jv::Vector<uint32_t> artifacts;
			jv::Vector<uint32_t> bosses;
			jv::Vector<uint32_t> rooms;
			jv::Vector<uint32_t> spells;
			jv::Vector<uint32_t> curses;
			jv::Vector<uint32_t> events;

			[[nodiscard]] static Decks Create(const LevelCreateInfo& info);
		} decks{};

		struct Path final
		{
			uint32_t boss = UINT32_MAX;
			uint32_t room = UINT32_MAX;
			uint32_t spell = UINT32_MAX;
			uint32_t artifact = UINT32_MAX;
			uint32_t curse = UINT32_MAX;
			uint32_t counters = 0;
		};

		uint32_t depth = 0;
		jv::Array<Path> paths;
		uint32_t chosenPath;

		jv::Vector<uint32_t> magicDeck;
		jv::Vector<uint32_t> hand;
		BoardState boardState;
		jv::Vector<ActionState> stack;

		uint32_t mana;
		uint32_t maxMana;
		uint32_t turn;

		uint32_t targets[BOARD_CAPACITY_PER_SIDE];
		bool tapped[BOARD_CAPACITY_PER_SIDE];

		void RemoveDuplicates(const LevelInfo& info, jv::Vector<uint32_t>& deck, uint32_t Path::* mem) const;
		[[nodiscard]] uint32_t GetMonster(const LevelInfo& info);
		[[nodiscard]] uint32_t GetBoss(const LevelInfo& info);
		[[nodiscard]] uint32_t GetRoom(const LevelInfo& info);
		[[nodiscard]] uint32_t GetSpell(const LevelInfo& info);
		[[nodiscard]] uint32_t GetArtifact(const LevelInfo& info);
		[[nodiscard]] uint32_t GetCurse(const LevelInfo& info);
		[[nodiscard]] uint32_t GetEvent(const LevelInfo& info, const uint32_t* ignore, uint32_t ignoreCount);
		[[nodiscard]] uint32_t Draw(const LevelInfo& info, const uint32_t* ignore, uint32_t ignoreCount);
		void ResetDeck(const LevelInfo& info);
		[[nodiscard]] uint32_t GetPrimaryPath() const;

		[[nodiscard]] static State Create(const LevelCreateInfo& info);
	};
}
