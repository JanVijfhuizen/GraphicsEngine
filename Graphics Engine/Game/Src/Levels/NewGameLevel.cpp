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
		constexpr glm::ivec2 headerPos{ SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y - 90 };

		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = headerPos;
		headerDrawInfo.text = "choose a mode";
		headerDrawInfo.center = true;
		headerDrawInfo.overflow = true;
		level->DrawHeader(info, headerDrawInfo);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = { headerPos.x, headerPos.y - 36 };
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
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y - 64 };
		headerDrawInfo.text = "choose your starting cards.";
		headerDrawInfo.center = true;
		headerDrawInfo.scale = 1;
		headerDrawInfo.overflow = true;
		level->DrawHeader(info, headerDrawInfo);

		if (monsterChoice != -1 && artifactChoice != -1)
		{
			if (timeSinceFirstChoicesMade < 0)
				timeSinceFirstChoicesMade = level->GetTime();

			headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, 64 };
			headerDrawInfo.text = "press enter to confirm your choice.";
			headerDrawInfo.overrideLifeTime = level->GetTime() - timeSinceFirstChoicesMade;
			level->DrawHeader(info, headerDrawInfo);

			if (info.inputState.enter.PressEvent())
			{
				state.monsterId = monsterChoice;
				state.artifactId = artifactChoice;
				stateIndex = 2;
			}
		}

		DiscoveredCardDrawInfo discoveredCardDrawInfo{};
		discoveredCardDrawInfo.highlighted = monsterChoice;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			discoveredCardDrawInfo.cards[i] = &info.monsters[monsterDiscoverOptions[i]];
		discoveredCardDrawInfo.height = SIMULATED_RESOLUTION.y / 2 + 18;
		const uint32_t discoveredMonster = DrawDiscoveredCards(info, discoveredCardDrawInfo);

		discoveredCardDrawInfo.highlighted = artifactChoice;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			discoveredCardDrawInfo.cards[i] = &info.artifacts[artifactDiscoverOptions[i]];
		discoveredCardDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - 18;
		const uint32_t discoveredArtifact = DrawDiscoveredCards(info, discoveredCardDrawInfo);

		if (discoveredMonster != -1)
			monsterChoice = discoveredMonster;
		if (discoveredArtifact != -1)
			artifactChoice = discoveredArtifact;

		return true;
	}

	bool NewGameLevel::JoinState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		TextTask joinTextTask{};
		joinTextTask.text = "daisy joins you on your adventure.";
		joinTextTask.position = TEXT_CENTER_TOP_POSITION;
		joinTextTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(joinTextTask);

		Card* cards = &info.monsters[0];

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = &cards;
		renderInfo.length = 1;
		RenderCards(renderInfo);

		TextTask textTask{};
		textTask.text = "press enter to continue.";
		textTask.scale = TEXT_BIG_SCALE;
		textTask.position = TEXT_CENTER_BOT_POSITION;
		info.textTasks.Push(textTask);

		if (info.inputState.enter.PressEvent())
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
