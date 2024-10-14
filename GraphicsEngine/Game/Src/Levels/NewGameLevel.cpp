#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "Levels/LevelUtils.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "Utils/Shuffle.h"

namespace game
{
	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);

		const auto states = jv::CreateArray<LevelState<State>*>(info.arena, 2);
		states[0] = info.arena.New<PartySelectState>();
		states[1] = info.arena.New<JoinState>();
		stateMachine = LevelStateMachine<State>::Create(info, states);
	}

	bool NewGameLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		return stateMachine.Update(info, this, loadLevelIndex);
	}

	bool NewGameLevel::PartySelectState::Create(State& state, const LevelCreateInfo& info)
	{
		timeUntilTextBubble = 10;
		textBubbleIndex = 0;
		monsterChoice = -1;
		timeSinceFirstChoicesMade = -1;
		for (auto& metaData : metaDatas)
			metaData = {};

		uint32_t count;
		GetDeck(nullptr, &count, info.monsters);
		monsterDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.artifacts);

		GetDeck(&monsterDeck, nullptr, info.monsters);

		// Create a discover option for your initial monster.
		monsterDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(monsterDeck.ptr, monsterDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			monsterDiscoverOptions[i] = monsterDeck.Pop();
		return true;
	}

	bool NewGameLevel::PartySelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex)
	{
		const char* pickLines[]
		{
			"pick me",
			"no me",
			"im stronger",
			"im op i swear",
			"dont pick them",
			"me me me"
		};

		timeUntilTextBubble -= info.deltaTime;
		if (timeUntilTextBubble < 0)
		{
			timeUntilTextBubble = 8;
			auto& metaData = metaDatas[rand() % 3];
			metaData.textBubble = pickLines[textBubbleIndex];
			metaData.textBubbleDuration = 0;
			metaData.textBubbleMaxDuration = 1.5f;

			textBubbleIndex++;
			if (textBubbleIndex >= 6)
				textBubbleIndex = 0;
		}
			
		level->DrawTopCenterHeader(info, HeaderSpacing::close, "choose your starting monster.");

		if (monsterChoice != -1)
		{
			if (timeSinceFirstChoicesMade < 0)
				timeSinceFirstChoicesMade = level->GetTime();

			level->DrawPressEnterToContinue(info, HeaderSpacing::close, level->GetTime() - timeSinceFirstChoicesMade);

			if (info.inputState.enter.PressEvent())
			{
				state.monsterId = monsterDiscoverOptions[monsterChoice];
				stateIndex = 1;
			}
		}

		Card* cards[DISCOVER_LENGTH]{};
		CombatStats combatStats[DISCOVER_LENGTH]{};

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.combatStats = combatStats;
		cardSelectionDrawInfo.lifeTime = level->GetTime();
		
		cardSelectionDrawInfo.highlighted = monsterChoice;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto monster = &info.monsters[monsterDiscoverOptions[i]];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
		}
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		cardSelectionDrawInfo.metaDatas = metaDatas;
		const uint32_t discoveredMonster = level->DrawCardSelection(info, cardSelectionDrawInfo);
		cardSelectionDrawInfo.combatStats = nullptr;
		
		if(info.inputState.lMouse.ReleaseEvent())
		{
			if (discoveredMonster != -1)
				monsterChoice = discoveredMonster;
		}

		return true;
	}

	void NewGameLevel::JoinState::Reset(State& state, const LevelInfo& info)
	{
		LevelState<State>::Reset(state, info);
		metaData = {};
	}

	bool NewGameLevel::JoinState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		auto& gameState = info.gameState;
		gameState.partySize = 2;
		gameState.artifactSlotCount = 1;
		gameState.monsterIds[0] = state.monsterId;
		gameState.monsterIds[1] = MONSTER_IDS::DAISY;

		for (uint32_t i = 0; i < PARTY_CAPACITY; ++i)
		{
			const auto& monster = info.monsters[gameState.monsterIds[i]];
			gameState.healths[i] = monster.health;
			for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
				gameState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j] = -1;
			gameState.curses[i] = -1;
		}
		
		for (uint32_t i = 0; i < 4; ++i)
			gameState.spells[i] = SPELL_IDS::ENRAGE;
		for (uint32_t i = 4; i < 8; ++i)
			gameState.spells[i] = SPELL_IDS::SHOCK;
		for (uint32_t i = 8; i < 12; ++i)
			gameState.spells[i] = SPELL_IDS::PROTECT;

		loadLevelIndex = LevelIndex::main;
		return true;
	}
}
