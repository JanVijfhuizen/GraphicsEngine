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

	uint32_t MainLevel::State::GetMonster(const LevelInfo& info)
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
		state.hand = jv::CreateVector<uint32_t>(info.arena, HAND_MAX_SIZE);
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
		level->DrawCardSelection(info, cardSelectionDrawInfo);

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
		uint32_t selected = level->DrawCardSelection(info, cardSelectionDrawInfo);
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
		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		turnState = TurnState::startOfTurn;
		selectionState = SelectionState::none;
		time = 0;
		auto& boardState = state.boardState = {};
		boardState.allyCount = boardState.partyCount = gameState.partySize;
		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const auto partyId = gameState.partyIds[i];
			const auto monsterId = playerState.monsterIds[partyId];
			boardState.ids[i] = monsterId;
			boardState.partyIds[i] = partyId;
			boardState.combatStats[i] = GetCombatStat(info.monsters[monsterId]);
			boardState.combatStats[i].health = gameState.healths[i];
		}
			
		selectedId = -1;

		const uint32_t layerIndex = jv::Min(state.depth / ROOM_COUNT_BEFORE_BOSS, TOTAL_BOSS_COUNT - 1);
		const uint32_t enemyCount = MONSTER_CAPACITIES[layerIndex];

		boardState.enemyCount = enemyCount;
		for (uint32_t i = 0; i < boardState.enemyCount; ++i)
		{
			const auto ind = BOARD_CAPACITY_PER_SIDE + i;
			auto& id = boardState.ids[ind];
			id = state.GetMonster(info);
			boardState.combatStats[ind] = GetCombatStat(info.monsters[id]);
		}
	}

	bool MainLevel::CombatState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		time += info.deltaTime;

		auto& gameState = info.gameState;
		auto& boardState = state.boardState;

		// Check health.
		for (int32_t i = static_cast<int32_t>(boardState.allyCount) - 1; i >= 0; --i)
		{
			if (boardState.combatStats[i].health == 0)
			{
				// Remove.
				for (uint32_t j = i; j < boardState.allyCount; ++j)
				{
					boardState.ids[j] = boardState.ids[j + 1];
					boardState.partyIds[j] = boardState.partyIds[j + 1];
					boardState.combatStats[j] = boardState.combatStats[j + 1];
				}
				--boardState.allyCount;
				if(boardState.partyCount >= i + 1)
					--boardState.partyCount;
			}
		}

		for (int32_t i = static_cast<int32_t>(boardState.enemyCount) - 1; i >= 0; --i)
		{
			if (boardState.combatStats[BOARD_CAPACITY_PER_SIDE + i].health == 0)
			{
				// Remove.
				for (uint32_t j = i; j < boardState.enemyCount; ++j)
				{
					boardState.ids[BOARD_CAPACITY_PER_SIDE + j] = boardState.ids[BOARD_CAPACITY_PER_SIDE + j + 1];
					boardState.combatStats[BOARD_CAPACITY_PER_SIDE + j] = boardState.combatStats[BOARD_CAPACITY_PER_SIDE + j + 1];
				}
				--boardState.enemyCount;
			}
		}

		// Check for game over.
		if (boardState.allyCount == 0)
		{
			loadLevelIndex = LevelIndex::mainMenu;
			return true;
		}

		// Check for room victory.
		if(boardState.enemyCount == 0)
		{
			gameState.partySize = boardState.partyCount;
			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				gameState.partyIds[i] = boardState.partyIds[i];
				gameState.healths[i] = boardState.combatStats[i].health;
			}

			stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);
			return true;
		}

		const auto& path = state.paths[state.chosenPath];

		{
			bool newTurn = true;
			for (uint32_t i = 0; i < boardState.allyCount; ++i)
			{
				if (!tapped[i])
					newTurn = false;
			}
			if (newTurn)
			{
				for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				{
					const uint32_t target = targets[i];
					if(boardState.allyCount >= target)
						Attack(state, BOARD_CAPACITY_PER_SIDE + i, target);
				}

				turnState = TurnState::startOfTurn;
			}
		}

		if(turnState == TurnState::startOfTurn)
		{
			// Set new random enemy targets.
			eventCard = state.GetEvent(info);
			for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				targets[i] = rand() % boardState.allyCount;

			// Untap.
			for (auto& i : tapped)
				i = false;

			mana = MAX_MANA;

			// Draw new hand.
			state.hand.Clear();
			for (uint32_t i = 0; i < HAND_MAX_SIZE; ++i)
				state.hand.Add() = state.Draw(info);
			turnState = TurnState::playerTurn;
		}

		const auto& lMouse = info.inputState.lMouse;
		const bool lMousePressed = lMouse.PressEvent();
		const bool lMouseReleased = lMouse.ReleaseEvent();

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = &info.rooms[path.room];
		cardDrawInfo.origin = glm::ivec2(8);
		level->DrawCard(info, cardDrawInfo);
		cardDrawInfo.card = &info.events[eventCard];
		cardDrawInfo.origin.x += 28;
		level->DrawCard(info, cardDrawInfo);

		constexpr uint32_t l = HAND_MAX_SIZE > BOARD_CAPACITY_PER_SIDE ? HAND_MAX_SIZE : BOARD_CAPACITY_PER_SIDE;

		Card* cards[l]{};
		for (uint32_t i = 0; i < boardState.enemyCount; ++i)
			cards[i] = &info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]];
		
		bool* selectedArr = nullptr;

		if(selectionState == SelectionState::enemy)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		// Draw enemies.
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.lifeTime = level->GetTime();
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = boardState.enemyCount;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 5 * 4;
		cardSelectionDrawInfo.costs = targets;
		cardSelectionDrawInfo.selectedArr = selectedArr;
		cardSelectionDrawInfo.combatStats = &boardState.combatStats[BOARD_CAPACITY_PER_SIDE];
		const auto enemyResult = level->DrawCardSelection(info, cardSelectionDrawInfo);
		selectedArr = nullptr;

		if (lMousePressed && enemyResult != -1)
		{
			selectionState = SelectionState::enemy;
			selectedId = enemyResult;
		}

		if (selectionState == SelectionState::hand)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		bool greyedOutArr[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
			greyedOutArr[i] = info.magics[state.hand[i]].cost > mana;

		// Draw hand.
		uint32_t costs[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
		{
			const auto magic = &info.magics[state.hand[i]];
			cards[i] = magic;
			costs[i] = magic->cost;
		}

		cardSelectionDrawInfo.length = state.hand.count;
		cardSelectionDrawInfo.height = 32;
		cardSelectionDrawInfo.texts = nullptr;
		cardSelectionDrawInfo.offsetMod = -4;
		cardSelectionDrawInfo.selectedArr = selectedArr;
		cardSelectionDrawInfo.greyedOutArr = greyedOutArr;
		cardSelectionDrawInfo.costs = costs;
		cardSelectionDrawInfo.combatStats = nullptr;
		const auto handResult = level->DrawCardSelection(info, cardSelectionDrawInfo);
		selectedArr = nullptr;

		if(lMousePressed && handResult != -1)
		{
			selectionState = SelectionState::hand;
			selectedId = handResult;
		}

		if (selectionState == SelectionState::ally)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		// Draw allies.
		const auto& playerState = info.playerState;

		Card* playerCards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < boardState.allyCount; ++i)
			playerCards[i] = &info.monsters[boardState.ids[i]];

		Card** stacks[PARTY_CAPACITY]{};
		uint32_t stacksCount[PARTY_CAPACITY]{};

		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const auto partyId = boardState.partyIds[i];

			if(boardState.partyCount <= i)
			{
				stacksCount[i] = 0;
				continue;
			}

			const uint32_t flaw = gameState.flaws[i];
			const uint32_t artifactCount = playerState.artifactSlotCounts[partyId];
			const uint32_t count = stacksCount[i] = (flaw != -1) + artifactCount;

			if (count == 0)
				continue;
			const auto arr = jv::CreateArray<Card*>(info.frameArena, count);
			stacks[i] = arr.ptr;
			for (uint32_t j = 0; j < artifactCount; ++j)
			{
				const uint32_t index = playerState.artifacts[partyId * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
			if(flaw != -1)
				stacks[i][artifactCount] = &info.flaws[flaw];
		}
		
		CardSelectionDrawInfo playerCardSelectionDrawInfo{};
		playerCardSelectionDrawInfo.cards = playerCards;
		playerCardSelectionDrawInfo.length = boardState.allyCount;
		playerCardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 5 * 2;
		playerCardSelectionDrawInfo.stacks = stacks;
		playerCardSelectionDrawInfo.stackCounts = stacksCount;
		playerCardSelectionDrawInfo.lifeTime = level->GetTime();
		playerCardSelectionDrawInfo.greyedOutArr = tapped;
		playerCardSelectionDrawInfo.combatStats = boardState.combatStats;
		playerCardSelectionDrawInfo.selectedArr = selectedArr;
		const auto allyResult = level->DrawCardSelection(info, playerCardSelectionDrawInfo);
		
		if (lMousePressed && allyResult != -1)
		{
			selectionState = SelectionState::ally;
			selectedId = allyResult;
		}

		// Draw mana.
		{
			TextTask manaTextTask{};
			manaTextTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, 4);
			manaTextTask.text = TextInterpreter::IntToConstCharPtr(mana, info.frameArena);
			manaTextTask.lifetime = level->GetTime();
			manaTextTask.center = true;
			info.textTasks.Push(manaTextTask);
		}

		if (lMouseReleased)
		{
			// If player tried to attack an enemy.
			if (selectionState == SelectionState::ally)
			{
				auto& isTapped = tapped[selectedId];
				if (!isTapped && enemyResult != -1)
				{
					// Do the attack.
					isTapped = true;
					Attack(state, selectedId, BOARD_CAPACITY_PER_SIDE + enemyResult);
				}
			}
			else if(selectionState == SelectionState::hand)
			{
				// Do something with the target.
				const auto& card = info.magics[selectedId];
				const bool validAll = card.type == MagicCard::Type::all && info.inputState.mousePos.y > SIMULATED_RESOLUTION.y / 4;
				const bool validTarget = card.type == MagicCard::Type::target && (enemyResult != -1 || allyResult != -1);
				const bool validPlay = validAll || validTarget;

				if(validPlay)
				{
					// Play card.
					if (card.cost > mana)
						mana = 0;
					else
						mana -= card.cost;

					state.hand.RemoveAtOrdered(selectedId);
				}
			}

			selectionState = SelectionState::none;
		}
			
		return true;
	}

	void MainLevel::CombatState::Attack(State& state, const uint32_t src, const uint32_t dst)
	{
		auto& boardState = state.boardState;
		auto& health = boardState.combatStats[dst].health;
		const auto& attack = boardState.combatStats[src].attack;;
		health = health < attack ? 0 : health - attack;
	}

	void MainLevel::RewardMagicCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
	}

	bool MainLevel::RewardMagicCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_DECK_SIZE];
		uint32_t costs[MAGIC_DECK_SIZE];
		for (uint32_t i = 0; i < MAGIC_DECK_SIZE; ++i)
		{
			const auto c = &info.magics[info.gameState.magics[i]];
			cards[i] = c;
			costs[i] = c->cost;
		}

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y / 2 + 40;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.length = MAGIC_DECK_SIZE;
		cardSelectionDrawInfo.costs = costs;
		const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		const auto& path = state.paths[state.chosenPath];

		const auto rewardCard = &info.magics[path.magic];
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = rewardCard;
		cardDrawInfo.center = true;
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y / 2 + 44 };
		cardDrawInfo.lifeTime = level->GetTime();
		cardDrawInfo.cost = rewardCard->cost;
		level->DrawCard(info, cardDrawInfo);

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

			Card* cards[PARTY_ACTIVE_CAPACITY];
			CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const auto monster = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];
				cards[i] = monster;
				combatStats[i] = GetCombatStat(*monster);
			}

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
			cardSelectionDrawInfo.combatStats = combatStats;
			const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
			
			const auto& path = state.paths[state.chosenPath];

			CardDrawInfo cardDrawInfo{};
			cardDrawInfo.card = &info.flaws[path.flaw];
			cardDrawInfo.center = true;
			cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
			cardDrawInfo.lifeTime = level->GetTime();
			level->DrawCard(info, cardDrawInfo);

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
		{
			auto& playerState = info.playerState;
			const auto& gameState = info.gameState;

			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const uint32_t id = gameState.partyIds[i];
				auto& slotCount = playerState.artifactSlotCounts[id];
				slotCount = jv::Max(slotCount, state.depth / ROOM_COUNT_BEFORE_BOSS);
			}
		}
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
		CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const uint32_t id = gameState.partyIds[i];
			const auto monster = &info.monsters[playerState.monsterIds[id]];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
			combatStats[i].health = gameState.healths[i];

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
		cardSelectionDrawInfo.combatStats = combatStats;
		const auto choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		auto& path = state.paths[state.chosenPath];
		
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
		cardDrawInfo.center = true;
		cardDrawInfo.lifeTime = level->GetTime();
		level->DrawCard(info, cardDrawInfo);
		
		if(choice != -1 && outStackSelected != -1)
		{
			const uint32_t id = gameState.partyIds[choice];

			if (info.inputState.lMouse.PressEvent())
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
			stateIndex = static_cast<uint32_t>(state.depth % ROOM_COUNT_BEFORE_BOSS == 0 ? StateNames::bossReveal : StateNames::pathSelect);
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
	}

	bool MainLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		return stateMachine.Update(info, this, loadLevelIndex);
	}
}
