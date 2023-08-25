#pragma once
#include "BoardState.h"
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
}
