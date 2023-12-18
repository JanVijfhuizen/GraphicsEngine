#include "pch_game.h"
#include "States/CombatState.h"
#include "Levels/LevelUtils.h"
#include "Levels/MainLevel.h"
#include <Utils/Shuffle.h>
#include "States/GameState.h"

namespace game
{
	State::Decks State::Decks::Create(const LevelCreateInfo& info)
	{
		Decks decks{};
		uint32_t count;
		
		GetDeck(nullptr, &count, info.rooms);
		decks.rooms = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.spells);
		decks.spells = jv::CreateVector<uint32_t>(info.arena, count * SPELL_CARD_COPY_COUNT);
		GetDeck(nullptr, &count, info.curses);
		decks.curses = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.events);
		decks.events = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.monsters);
		decks.monsters = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.artifacts);
		decks.artifacts = jv::CreateVector<uint32_t>(info.arena, count);

		return decks;
	}

	void State::RemoveDuplicates(const LevelInfo& info, jv::Vector<uint32_t>& deck, uint32_t Path::* mem) const
	{
		for (const auto& path : paths)
			for (uint32_t i = 0; i < deck.count; ++i)
				if (deck[i] == path.*mem)
				{
					deck.RemoveAt(i);
					break;
				}
	}

	uint32_t State::GetMonster(const LevelInfo& info)
	{
		auto& monsters = decks.monsters;
		if (monsters.count == 0)
		{
			GetDeck(&monsters, nullptr, info.monsters);

			for (int32_t i = static_cast<int32_t>(monsters.count) - 1; i >= 0; --i)
			{
				bool removed = false;

				for (uint32_t j = 0; j < boardState.allyCount; ++j)
					if (monsters[i] == boardState.ids[j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < boardState.enemyCount; ++j)
					if (monsters[i] == boardState.ids[BOARD_CAPACITY_PER_SIDE + j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < info.gameState.partySize; ++j)
					if (monsters[i] == info.gameState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}

				if (removed)
					continue;

				for (auto& as: stack)
				{
					if (as.trigger != ActionState::Trigger::onSummon)
						continue;
					if(as.values[static_cast<uint32_t>(ActionState::VSummon::id)] == monsters[i])
					{
						monsters.RemoveAt(i);
						break;
					}
				}
			}

			Shuffle(decks.monsters.ptr, decks.monsters.count);
		}
		return monsters.Pop();
	}

	uint32_t State::GetBoss(const LevelInfo& info, const uint32_t path) const
	{
		uint32_t l = depth / ROOM_COUNT_BEFORE_BOSS;
		if (info.bosses.length <= l)
			return -1;
		return info.bosses[l * 3 + path];
	}

	uint32_t State::GetRoom(const LevelInfo& info)
	{
		auto& rooms = decks.rooms;
		if (rooms.count == 0)
		{
			GetDeck(&rooms, nullptr, info.rooms);
			RemoveDuplicates(info, rooms, &Path::room);
			Shuffle(decks.rooms.ptr, decks.rooms.count);

			for (int32_t i = static_cast<int32_t>(decks.rooms.count) - 1; i >= 0; --i)
			{
				const auto id = decks.rooms[i];
				const auto& room = info.rooms[id];
				if (room.unique)
					decks.rooms.RemoveAt(i);
			}
		}
		return rooms.Pop();
	}

	uint32_t State::GetSpell(const LevelInfo& info)
	{
		auto& magics = decks.spells;
		if (magics.count == 0)
		{
			GetDeck(&magics, nullptr, info.spells);
			RemoveDuplicates(info, magics, &Path::spell);
			Shuffle(decks.spells.ptr, decks.spells.count);
			RemoveMagicsInParty(magics, info.gameState);
		}
		return magics.Pop();
	}

	uint32_t State::GetArtifact(const LevelInfo& info)
	{
		auto& artifacts = decks.artifacts;
		if (artifacts.count == 0)
		{
			GetDeck(&artifacts, nullptr, info.artifacts);
			RemoveDuplicates(info, artifacts, &Path::artifact);
			Shuffle(decks.artifacts.ptr, decks.artifacts.count);
			RemoveArtifactsInParty(artifacts, info.gameState);
		}
		return artifacts.Pop();
	}

	uint32_t State::GetCurse(const LevelInfo& info)
	{
		auto& flaws = decks.curses;
		if (flaws.count == 0)
		{
			GetDeck(&flaws, nullptr, info.curses);
			RemoveDuplicates(info, flaws, &Path::curse);
			Shuffle(decks.curses.ptr, decks.curses.count);
			RemoveFlawsInParty(flaws, info.gameState);
		}
		return flaws.Pop();
	}

	uint32_t State::GetEvent(const LevelInfo& info, const uint32_t* ignore, const uint32_t ignoreCount)
	{
		auto& events = decks.events;
		if (events.count == 0)
		{
			GetDeck(&events, nullptr, info.events);
			for (auto i = static_cast<int32_t>(events.count) - 1; i >= 0; --i)
			{
				bool ignored = false;
				for (uint32_t j = 0; j < ignoreCount; ++j)
				{
					if (events[i] == ignore[j])
					{
						ignored = true;
						break;
					}
				}
				if(ignored)
					events.RemoveAt(i);
			}

			Shuffle(decks.events.ptr, decks.events.count);
		}
		return events.Pop();
	}

	uint32_t State::Draw(const LevelInfo& info, const uint32_t* ignore, const uint32_t ignoreCount)
	{
		if (magicDeck.count == 0)
		{
			ResetDeck(info);
			for (auto i = static_cast<int32_t>(magicDeck.count) - 1; i >= 0; --i)
			{
				bool ignored = false;
				for (uint32_t j = 0; j < ignoreCount; ++j)
				{
					if (magicDeck[i] == ignore[j])
					{
						ignored = true;
						break;
					}
				}
				if (ignored)
					magicDeck.RemoveAt(i);
			}
		}
		return magicDeck.Pop();
	}

	void State::ResetDeck(const LevelInfo& info)
	{
		magicDeck.Clear();
		for (const auto& magic : info.gameState.spells)
			magicDeck.Add() = magic;
		for (int32_t i = static_cast<int32_t>(magicDeck.count) - 1; i >= 0; --i)
			for (const auto& card : hand)
				if(magicDeck[i] == card)
				{
					magicDeck.RemoveAt(i);
					break;
				}

		Shuffle(magicDeck.ptr, magicDeck.count);
	}

	uint32_t State::GetPrimaryPath() const
	{
		uint32_t counters = 0;
		uint32_t index = 0;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto path = paths[i];
			if (path.counters > counters)
			{
				counters = path.counters;
				index = i;
			}
		}

		return index;
	}

	State State::Create(const LevelCreateInfo& info)
	{
		State state{};
		state.decks = Decks::Create(info);
		state.paths = jv::CreateArray<Path>(info.arena, DISCOVER_LENGTH);
		state.magicDeck = jv::CreateVector<uint32_t>(info.arena, SPELL_DECK_SIZE);
		state.hand = jv::CreateVector<uint32_t>(info.arena, HAND_MAX_SIZE);
		state.stack = jv::CreateVector<ActionState>(info.arena, STACK_MAX_SIZE);
		state.mana = 0;
		state.maxMana = 0;
		return state;
	}

	void State::TryAddToStack(const ActionState& actionState)
	{
		if(stack.count < stack.length)
			stack.Add() = actionState;
	}
}
