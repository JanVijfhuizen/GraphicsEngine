#include "pch_game.h"
#include "Levels/MainLevel.h"
#include <Levels/LevelUtils.h>

#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Math.h"
#include "States/InputState.h"
#include "States/GameState.h"
#include "States/BoardState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game
{
	MainLevel::State::Decks MainLevel::State::Decks::Create(const LevelCreateInfo& info)
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

	void MainLevel::State::RemoveDuplicates(const LevelInfo& info, jv::Vector<uint32_t>& deck, uint32_t Path::* mem) const
	{
		for (const auto& path : paths)
			for (uint32_t i = 0; i < deck.count; ++i)
				if (deck[i] == path.*mem)
				{
					deck.RemoveAt(i);
					break;
				}
	}

	uint32_t MainLevel::State::GetMonster(const LevelInfo& info, const BoardState& boardState)
	{
		auto& monsters = decks.monsters;
		if (monsters.count == 0)
		{
			GetDeck(&monsters, nullptr, info.monsters);

			for (int32_t i = static_cast<int32_t>(monsters.count) - 1; i >= 0; --i)
			{
				bool removed = false;

				for (uint32_t j = 0; j < boardState.alliedMonsterCount; ++j)
					if (monsters[i] == boardState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < boardState.enemyMonsterCount; ++j)
					if (monsters[i] == boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < info.playerState.partySize; ++j)
					if(monsters[i] == info.playerState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						break;
					}
			}
		}
		return monsters.Pop();
	}

	uint32_t MainLevel::State::GetBoss(const LevelInfo& info)
	{
		auto& bosses = decks.bosses;
		if (bosses.count == 0)
		{
			GetDeck(&bosses, nullptr, info.bosses);
			RemoveDuplicates(info, bosses, &Path::boss);
		}
		return bosses.Pop();
	}

	uint32_t MainLevel::State::GetRoom(const LevelInfo& info)
	{
		auto& rooms = decks.rooms;
		if (rooms.count == 0)
		{
			GetDeck(&rooms, nullptr, info.rooms);
			RemoveDuplicates(info, rooms, &Path::room);
		}
		return rooms.Pop();
	}

	uint32_t MainLevel::State::GetMagic(const LevelInfo& info)
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

	uint32_t MainLevel::State::GetArtifact(const LevelInfo& info)
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

	uint32_t MainLevel::State::GetFlaw(const LevelInfo& info)
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

	uint32_t MainLevel::State::GetEvent(const LevelInfo& info)
	{
		auto& events = decks.events;
		if (events.count == 0)
			GetDeck(&events, nullptr, info.flaws);
		return events.Pop();
	}

	uint32_t MainLevel::State::Draw(const LevelInfo& info)
	{
		if(magicDeck.count == 0)
		{
			for (const auto& magic : info.gameState.magics)
				magicDeck.Add() = magic;
			Shuffle(magicDeck.ptr, magicDeck.count);
		}

		return magicDeck.Pop();
	}

	uint32_t MainLevel::State::GetPrimaryPath() const
	{
		uint32_t counters = 0;
		uint32_t index = 0;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto path = paths[i];
			if(path.counters > counters)
			{
				counters = path.counters;
				index = i;
			}
		}

		return index;
	}

	MainLevel::State MainLevel::State::Create(const LevelCreateInfo& info)
	{
		State state{};
		state.decks = Decks::Create(info);
		state.paths = jv::CreateArray<Path>(info.arena, DISCOVER_LENGTH);
		state.magicDeck = jv::CreateVector<uint32_t>(info.arena, MAGIC_DECK_SIZE);
		state.hand = jv::CreateVector<uint32_t>(info.arena, HAND_DRAW_COUNT);
		return state;
	}

	void MainLevel::BossRevealState::Reset(State& state, const LevelInfo& info)
	{
		for (auto& path : state.paths)
		{
			path = {};
			path.boss = state.GetBoss(info);
		}
	}

	bool MainLevel::BossRevealState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.bosses[state.paths[i].boss];

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		DrawCardSelection(info, cardSelectionDrawInfo);

		const char* text = "the stage bosses have been revealed.";
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);
		if(f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, f);
		
		if (info.inputState.enter.PressEvent())
			stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		return true;
	}

	void MainLevel::PathSelectState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;

		const bool addFlaw = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1;
		const bool addArtifact = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1;

		for (auto& path : state.paths)
		{
			path.room = state.GetRoom(info);
			path.magic = state.GetMagic(info);
			if (addFlaw)
				path.flaw = state.GetFlaw(info);
			if (addArtifact)
				path.artifact = state.GetArtifact(info);
		}
	}

	bool MainLevel::PathSelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const bool flawPresent = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1;
		const bool bossPresent = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1;

		// Render bosses.
		Card* cards[DISCOVER_LENGTH + 1];
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto& path = state.paths[i];
			if(!bossPresent)
				cards[i] = &info.bosses[path.boss];
			else
				cards[i] = &info.artifacts[path.artifact];
		}
		if (bossPresent)
			cards[DISCOVER_LENGTH] = &info.bosses[state.paths[state.GetPrimaryPath()].boss];

		Card** stacks[DISCOVER_LENGTH]{};
		for (auto& stack : stacks)
			stack = jv::CreateArray<Card*>(info.frameArena, 3).ptr;

		uint32_t stacksCounts[DISCOVER_LENGTH + 1]{};
		for (auto& c : stacksCounts)
			c = 2 + flawPresent;
		stacksCounts[DISCOVER_LENGTH] = 0;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto& stack = stacks[i];
			const auto& path = state.paths[i];

			stack[0] = &info.rooms[path.room];
			stack[1] = &info.magics[path.magic];
			if (flawPresent)
				stack[2] = &info.flaws[path.flaw];
		}

		const char* texts[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			uint32_t counters = state.paths[i].counters;
			counters += i == discoverOption;
			texts[i] = TextInterpreter::IntToConstCharPtr(counters, info.frameArena);
		}
		
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH + bossPresent;
		cardSelectionDrawInfo.stacks = stacks;
		cardSelectionDrawInfo.stackCounts = stacksCounts;
		cardSelectionDrawInfo.texts = bossPresent ? nullptr : texts;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		cardSelectionDrawInfo.highlighted = discoverOption;
		uint32_t selected = DrawCardSelection(info, cardSelectionDrawInfo);
		if (selected == DISCOVER_LENGTH)
			selected = -1;

		if (info.inputState.lMouse.PressEvent())
		{
			discoverOption = selected == discoverOption ? -1 : selected;
			if (selected != -1)
				timeSinceDiscovered = level->GetTime();
		}

		const char* text = "select which road to take.";
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);
		
		if (discoverOption != -1)
		{
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, level->GetTime() - timeSinceDiscovered);
			
			if (info.inputState.enter.PressEvent())
			{
				state.chosenPath = discoverOption;
				auto& path = state.paths[discoverOption];
				++path.counters;
				++state.depth;

				stateIndex = static_cast<uint32_t>(StateNames::combat);
			}
		}

		return true;
	}

	void MainLevel::CombatState::Reset(State& state, const LevelInfo& info)
	{
		
	}

	bool MainLevel::CombatState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);
		return true;
	}

	void MainLevel::RewardMagicCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
	}

	bool MainLevel::RewardMagicCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_DECK_SIZE]{};
		for (uint32_t i = 0; i < MAGIC_DECK_SIZE; ++i)
			cards[i] = &info.magics[info.gameState.magics[i]];

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y / 2 + 40;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.length = MAGIC_DECK_SIZE;
		const uint32_t choice = DrawCardSelection(info, cardSelectionDrawInfo);
		
		const auto& path = state.paths[state.chosenPath];

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = &info.magics[path.magic];
		cardDrawInfo.center = true;
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y / 2 + 44 };
		DrawCard(info, cardDrawInfo);

		if (info.inputState.lMouse.PressEvent())
			discoverOption = choice == discoverOption ? -1 : choice;

		const char* text = "select a card to replace, if any.";
		level->DrawTopCenterHeader(info, HeaderSpacing::normal, text);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, f);
		
		if (info.inputState.enter.PressEvent())
		{
			if (discoverOption != -1)
				info.gameState.magics[discoverOption] = path.magic;

			if (state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW)
				stateIndex = static_cast<uint32_t>(StateNames::rewardFlaw);
			else if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
				stateIndex = static_cast<uint32_t>(StateNames::rewardArtifact);
			else
				stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		}

		return true;
	}

	void MainLevel::RewardFlawCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
	}

	bool MainLevel::RewardFlawCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		bool greyedOut[PARTY_ACTIVE_CAPACITY];
		bool flawSlotAvailable = false;

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const auto& flaw = info.gameState.flaws[i];
			greyedOut[i] = flaw != -1;
			flawSlotAvailable = flawSlotAvailable ? true : flaw == -1;
		}

		if (flawSlotAvailable)
		{
			level->DrawTopCenterHeader(info, HeaderSpacing::normal, "select one ally to carry this flaw.");

			Card* cards[MAGIC_DECK_SIZE]{};
			for (uint32_t i = 0; i < gameState.partySize; ++i)
				cards[i] = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];

			Card** stacks[PARTY_CAPACITY]{};
			uint32_t stackCounts[PARTY_CAPACITY]{};

			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const uint32_t count = stackCounts[i] = playerState.artifactSlotCounts[i];
				if (count == 0)
					continue;
				const auto arr = jv::CreateArray<Card*>(info.frameArena, count + 1);
				stacks[i] = arr.ptr;
				for (uint32_t j = 0; j < count; ++j)
				{
					const uint32_t index = playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
					arr[j] = index == -1 ? nullptr : &info.artifacts[index];
				}

				const uint32_t flaw = info.gameState.flaws[i];
				if (flaw != -1)
				{
					++stackCounts[i];
					stacks[i][count] = &info.flaws[flaw];
				}
			}

			CardSelectionDrawInfo cardSelectionDrawInfo{};
			cardSelectionDrawInfo.cards = cards;
			cardSelectionDrawInfo.length = gameState.partySize;
			cardSelectionDrawInfo.greyedOutArr = greyedOut;
			cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
			cardSelectionDrawInfo.stacks = stacks;
			cardSelectionDrawInfo.stackCounts = stackCounts;
			cardSelectionDrawInfo.highlighted = discoverOption;
			const uint32_t choice = DrawCardSelection(info, cardSelectionDrawInfo);
			
			const auto& path = state.paths[state.chosenPath];

			CardDrawInfo cardDrawInfo{};
			cardDrawInfo.card = &info.flaws[path.flaw];
			cardDrawInfo.center = true;
			cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
			DrawCard(info, cardDrawInfo);

			if (info.inputState.lMouse.PressEvent())
			{
				discoverOption = choice == discoverOption ? -1 : choice;
				if (discoverOption != -1)
					timeSinceDiscovered = level->GetTime();
			}

			if (discoverOption == -1)
				return true;
			
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, level->GetTime() - timeSinceDiscovered);
			if (!info.inputState.enter.PressEvent())
				return true;

			info.gameState.flaws[gameState.partyIds[discoverOption]] = path.flaw;
		}

		stateIndex = static_cast<uint32_t>(StateNames::exitFound);
		return true;
	}

	void MainLevel::RewardArtifactState::Reset(State& state, const LevelInfo& info)
	{
		if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
			for (auto& slotCount : info.playerState.artifactSlotCounts)
				slotCount = jv::Max(slotCount, state.depth / ROOM_COUNT_BEFORE_BOSS);
	}

	bool MainLevel::RewardArtifactState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		Card* cards[PARTY_ACTIVE_CAPACITY]{};
		
		auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		const char* text = "you have the chance to equip this new artifact, and swap your artifacts around.";
		level->DrawTopCenterHeader(info, HeaderSpacing::normal, text);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, f);

		for (uint32_t i = 0; i < gameState.partySize; ++i)
			cards[i] = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];
		
		Card** artifacts[PARTY_ACTIVE_CAPACITY]{};
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const uint32_t id = gameState.partyIds[i];
			const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[id];
			if (count == 0)
				continue;
			const auto arr = artifacts[i] = jv::CreateArray<Card*>(info.frameArena, MONSTER_ARTIFACT_CAPACITY).ptr;
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = playerState.artifacts[id * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		uint32_t outStackSelected;
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = gameState.partySize;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
		cardSelectionDrawInfo.stacks = artifacts;
		cardSelectionDrawInfo.stackCounts = artifactCounts;
		cardSelectionDrawInfo.outStackSelected = &outStackSelected;
		const auto choice = DrawCardSelection(info, cardSelectionDrawInfo);
		
		auto& path = state.paths[state.chosenPath];
		
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
		cardDrawInfo.center = true;
		DrawCard(info, cardDrawInfo);

		if(choice != -1)
		{
			const uint32_t id = gameState.partyIds[choice];

			if (info.inputState.lMouse.PressEvent() && outStackSelected != -1 &&
				outStackSelected < playerState.artifactSlotCounts[id])
			{
				const uint32_t swappable = path.artifact;
				path.artifact = playerState.artifacts[id * MONSTER_ARTIFACT_CAPACITY + outStackSelected];
				playerState.artifacts[id * MONSTER_ARTIFACT_CAPACITY + outStackSelected] = swappable;
			}
		}

		if (info.inputState.enter.PressEvent())
			stateIndex = static_cast<uint32_t>(StateNames::exitFound);

		return true;
	}

	bool MainLevel::ExitFoundState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		level->DrawTopCenterHeader(info, HeaderSpacing::close, "press onwards, or flee.");

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + 9 };
		buttonDrawInfo.text = "continue";
		buttonDrawInfo.center = true;

		if (level->DrawButton(info, buttonDrawInfo))
		{
			stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
			return true;
		}

		buttonDrawInfo.origin.y -= 18;
		buttonDrawInfo.text = "quit";
		if (level->DrawButton(info, buttonDrawInfo))
		{
			SaveData(info.playerState);
			loadLevelIndex = LevelIndex::mainMenu;
			return true;
		}

		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = -.18;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::fallback)].subTexture;
		info.renderTasks.Push(buttonRenderTask);

		return true;
	}

	void MainLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);

		if (info.playerState.ironManMode)
			ClearSaveData();

		const auto states = jv::CreateArray<LevelState<State>*>(info.arena, 7);
		states[0] = info.arena.New<BossRevealState>();
		states[1] = info.arena.New<PathSelectState>();
		states[2] = info.arena.New<CombatState>();
		states[3] = info.arena.New<RewardMagicCardState>();
		states[4] = info.arena.New<RewardFlawCardState>();
		states[5] = info.arena.New<RewardArtifactState>();
		states[6] = info.arena.New<ExitFoundState>();
		stateMachine = LevelStateMachine<State>::Create(info, states, State::Create(info));
		// temp
		stateMachine.state.depth = 9;
	}

	bool MainLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		return stateMachine.Update(info, this, loadLevelIndex);
	}
}
