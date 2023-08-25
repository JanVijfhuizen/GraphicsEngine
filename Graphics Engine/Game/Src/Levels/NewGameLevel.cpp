#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "Levels/LevelUtils.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/Shuffle.h"

namespace game
{
	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		ClearSaveData();

		const auto states = jv::CreateArray<LevelState<State>*>(info.arena, 3);
		states[0] = info.arena.New<ModeSelectState>();
		states[1] = info.arena.New<PartySelectState>();
		states[2] = info.arena.New<JoinState>();
		stateMachine = LevelStateMachine<State>::Create(info, states);
	}

	bool NewGameLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		return stateMachine.Update(info, this, loadLevelIndex);
	}

	bool NewGameLevel::ModeSelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex)
	{
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + 27 };
		headerDrawInfo.text = "choose a mode.";
		headerDrawInfo.center = true;
		level->DrawHeader(info, headerDrawInfo);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + 9 };
		buttonDrawInfo.text = "standard";
		buttonDrawInfo.center = true;

		if (level->DrawButton(info, buttonDrawInfo))
		{
			info.playerState.ironManMode = false;
			stateIndex = 1;
			return true;
		}

		buttonDrawInfo.origin.y -= 18;
		buttonDrawInfo.text = "iron man";
		if (level->DrawButton(info, buttonDrawInfo))
		{
			info.playerState.ironManMode = true;
			stateIndex = 1;
			return true;
		}

		return true;
	}

	bool NewGameLevel::PartySelectState::Create(State& state, const LevelCreateInfo& info)
	{
		monsterChoice = -1;
		artifactChoice = -1;
		timeSinceFirstChoicesMade = -1;

		uint32_t count;
		GetDeck(nullptr, &count, info.monsters);
		monsterDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.artifacts);
		artifactDeck = jv::CreateVector<uint32_t>(info.arena, count);

		GetDeck(&monsterDeck, nullptr, info.monsters);
		GetDeck(&artifactDeck, nullptr, info.artifacts);

		// Create a discover option for your initial monster.
		monsterDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(monsterDeck.ptr, monsterDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			monsterDiscoverOptions[i] = monsterDeck.Pop();
		// Create a discover option for your initial artifact.
		artifactDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(artifactDeck.ptr, artifactDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			artifactDiscoverOptions[i] = artifactDeck.Pop();
		return true;
	}

	bool NewGameLevel::PartySelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex)
	{
		level->DrawTopCenterHeader(info, HeaderSpacing::close, "choose your starting cards.");

		if (monsterChoice != -1 && artifactChoice != -1)
		{
			if (timeSinceFirstChoicesMade < 0)
				timeSinceFirstChoicesMade = level->GetTime();

			level->DrawPressEnterToContinue(info, HeaderSpacing::close, level->GetTime() - timeSinceFirstChoicesMade);

			if (info.inputState.enter.PressEvent())
			{
				state.monsterId = monsterDiscoverOptions[monsterChoice];
				state.artifactId = artifactDiscoverOptions[artifactChoice];
				stateIndex = 2;
			}
		}

		Card* cards[DISCOVER_LENGTH]{};
		CombatStats combatStats[DISCOVER_LENGTH]{};

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.combatStats = combatStats;

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];

		cardSelectionDrawInfo.highlighted = monsterChoice;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto monster = &info.monsters[monsterDiscoverOptions[i]];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
		}
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y / 2 + 2;
		const uint32_t discoveredMonster = level->DrawCardSelection(info, cardSelectionDrawInfo);
		cardSelectionDrawInfo.combatStats = nullptr;

		cardSelectionDrawInfo.highlighted = artifactChoice;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.artifacts[artifactDiscoverOptions[i]];
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y / 2 - 2;
		const uint32_t discoveredArtifact = level->DrawCardSelection(info, cardSelectionDrawInfo);

		if(info.inputState.lMouse.ReleaseEvent())
		{
			if (discoveredMonster != -1)
				monsterChoice = discoveredMonster;
			if (discoveredArtifact != -1)
				artifactChoice = discoveredArtifact;
		}

		return true;
	}

	bool NewGameLevel::JoinState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const char* text = "daisy joins you on your adventure.";
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);

		const auto monster = &info.monsters[0];
		auto combatStats = GetCombatStat(*monster);

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = monster;
		cardDrawInfo.center = true;
		cardDrawInfo.combatStats = &combatStats;
		cardDrawInfo.origin = SIMULATED_RESOLUTION / 2;
		level->DrawCard(info, cardDrawInfo);
		
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if(f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, f);

		if (!level->GetIsLoading() && info.inputState.enter.PressEvent())
		{
			auto& playerState = info.playerState = PlayerState::Create();
			playerState.AddMonster(state.monsterId);
			playerState.AddMonster(MONSTER_STARTING_COMPANION_ID);
			playerState.AddArtifact(0, state.artifactId);
			SaveData(playerState);
			loadLevelIndex = LevelIndex::partySelect;
		}
		return true;
	}
}
