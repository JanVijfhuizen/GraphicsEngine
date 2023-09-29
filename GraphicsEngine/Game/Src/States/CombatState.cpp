#include "pch_game.h"
#include "States/CombatState.h"
#include "Levels/LevelUtils.h"
#include "Levels/MainLevel.h"
#include <Utils/Shuffle.h>
#include "States/GameState.h"
#include "States/PlayerState.h"

namespace game
{
	State::Decks State::Decks::Create(const LevelCreateInfo& info)
	{
		Decks decks{};
		uint32_t count;
		
		decks.bosses = jv::CreateVector<uint32_t>(info.arena, info.bosses.length);
		GetDeck(nullptr, &count, info.rooms);
		decks.rooms = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.magics);
		decks.magics = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.flaws);
		decks.flaws = jv::CreateVector<uint32_t>(info.arena, count);
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
				if(info.monsters[i].unique)
				{
					monsters.RemoveAt(i);
					continue;
				}

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

				for (uint32_t j = 0; j < info.playerState.partySize; ++j)
					if (monsters[i] == info.playerState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						break;
					}
			}

			Shuffle(decks.monsters.ptr, decks.monsters.count);
		}
		return monsters.Pop();
	}

	uint32_t State::GetBoss(const LevelInfo& info)
	{
		auto& bosses = decks.bosses;
		if (bosses.count == 0)
		{
			memcpy(decks.bosses.ptr, info.bosses.ptr, decks.bosses.length * sizeof(uint32_t));
			decks.bosses.count = decks.bosses.length;
			RemoveDuplicates(info, bosses, &Path::boss);
			Shuffle(decks.bosses.ptr, decks.bosses.count);
		}
		return bosses.Pop();
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

	uint32_t State::GetMagic(const LevelInfo& info)
	{
		auto& magics = decks.magics;
		if (magics.count == 0)
		{
			GetDeck(&magics, nullptr, info.magics);
			RemoveDuplicates(info, magics, &Path::magic);
			Shuffle(decks.magics.ptr, decks.magics.count);

			for (int32_t i = static_cast<int32_t>(decks.magics.count) - 1; i >= 0; --i)
				if (info.magics[decks.magics[i]].unique)
					decks.magics.RemoveAt(i);
		}
		RemoveMagicsInParty(magics, info.gameState);
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

			for (int32_t i = static_cast<int32_t>(decks.artifacts.count) - 1; i >= 0; --i)
				if (info.artifacts[decks.artifacts[i]].unique)
					decks.artifacts.RemoveAt(i);
		}
		RemoveArtifactsInParty(artifacts, info.playerState);
		return artifacts.Pop();
	}

	uint32_t State::GetFlaw(const LevelInfo& info)
	{
		auto& flaws = decks.flaws;
		if (flaws.count == 0)
		{
			GetDeck(&flaws, nullptr, info.flaws);
			RemoveDuplicates(info, flaws, &Path::flaw);
			Shuffle(decks.flaws.ptr, decks.flaws.count);

			for (int32_t i = static_cast<int32_t>(decks.flaws.count) - 1; i >= 0; --i)
				if (info.flaws[decks.flaws[i]].unique)
					decks.flaws.RemoveAt(i);
		}
		RemoveFlawsInParty(flaws, info.gameState);
		return flaws.Pop();
	}

	uint32_t State::GetEvent(const LevelInfo& info)
	{
		auto& events = decks.events;
		if (events.count == 0)
		{
			GetDeck(&events, nullptr, info.flaws);
			Shuffle(decks.events.ptr, decks.events.count);

			for (int32_t i = static_cast<int32_t>(decks.events.count) - 1; i >= 0; --i)
				if (info.events[decks.events[i]].unique)
					decks.events.RemoveAt(i);
		}
		return events.Pop();
	}

	uint32_t State::Draw(const LevelInfo& info)
	{
		if (magicDeck.count == 0)
			ResetDeck(info);
		return magicDeck.Pop();
	}

	void State::ResetDeck(const LevelInfo& info)
	{
		magicDeck.Clear();
		for (const auto& magic : info.gameState.magics)
			magicDeck.Add() = magic;
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
		state.magicDeck = jv::CreateVector<uint32_t>(info.arena, MAGIC_DECK_SIZE);
		state.hand = jv::CreateVector<uint32_t>(info.arena, HAND_MAX_SIZE);
		state.stack = jv::CreateVector<ActionState>(info.arena, STACK_MAX_SIZE);
		state.mana = 0;
		state.maxMana = 0;
		return state;
	}
}
