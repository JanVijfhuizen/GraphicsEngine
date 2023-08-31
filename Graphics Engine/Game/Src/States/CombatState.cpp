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

		GetDeck(nullptr, &count, info.bosses);
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
		}
		return monsters.Pop();
	}

	uint32_t State::GetBoss(const LevelInfo& info)
	{
		auto& bosses = decks.bosses;
		if (bosses.count == 0)
		{
			GetDeck(&bosses, nullptr, info.bosses);
			RemoveDuplicates(info, bosses, &Path::boss);
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
		}
		RemoveFlawsInParty(flaws, info.gameState);
		return flaws.Pop();
	}

	uint32_t State::GetEvent(const LevelInfo& info)
	{
		auto& events = decks.events;
		if (events.count == 0)
			GetDeck(&events, nullptr, info.flaws);
		return events.Pop();
	}

	uint32_t State::Draw(const LevelInfo& info)
	{
		if (magicDeck.count == 0)
		{
			for (const auto& magic : info.gameState.magics)
				magicDeck.Add() = magic;
			Shuffle(magicDeck.ptr, magicDeck.count);
		}

		return magicDeck.Pop();
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
		return state;
	}
}