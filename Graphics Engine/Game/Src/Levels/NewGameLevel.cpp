#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "Levels/LevelUtils.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
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
		level->DrawHeader(info, headerPos, "choose a mode", true, true);

		const bool standardPressed = level->DrawButton(info, { SIMULATED_RESOLUTION.x / 2, headerPos.y - 36 }, "standard", true);
		const bool ironManPressed = level->DrawButton(info, { SIMULATED_RESOLUTION.x / 2, headerPos.y - 60 }, "iron man", true);

		if(standardPressed)
		{
			info.playerState.ironManMode = false;
			stateIndex = 1;
			return true;
		}
		if(ironManPressed)
		{
			info.playerState.ironManMode = true;
			stateIndex = 1;
			return true;
		}

		return true;
	}

	bool NewGameLevel::PartySelectState::Create(State& state, const LevelCreateInfo& info)
	{
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
		Card* cards[DISCOVER_LENGTH]{};

		RenderMonsterCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = cards;
		renderInfo.length = DISCOVER_LENGTH;
		renderInfo.position = glm::vec2(0, -CARD_HEIGHT_OFFSET);
		renderInfo.highlight = monsterChoice;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.monsters[monsterDiscoverOptions[i]];
		auto choice = RenderMonsterCards(info.frameArena, renderInfo).selectedMonster;
		if (info.inputState.lMouse == InputState::pressed && choice != -1)
			monsterChoice = choice == monsterChoice ? -1 : choice;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.artifacts[artifactDiscoverOptions[i]];
		renderInfo.position.y *= -1;
		renderInfo.highlight = artifactChoice;
		choice = RenderCards(renderInfo);
		if (info.inputState.lMouse == InputState::pressed && choice != -1)
			artifactChoice = choice == artifactChoice ? -1 : choice;

		TextTask buttonTextTask{};
		buttonTextTask.text = "choose your starting cards.";
		buttonTextTask.position = TEXT_CENTER_TOP_POSITION;
		buttonTextTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(buttonTextTask);

		if (monsterChoice != -1 && artifactChoice != -1)
		{
			TextTask textTask{};
			textTask.text = "press enter to confirm your choice.";
			textTask.position = TEXT_CENTER_BOT_POSITION;
			textTask.scale = TEXT_BIG_SCALE;
			info.textTasks.Push(textTask);

			if (info.inputState.enter == InputState::pressed)
			{
				state.monsterId = monsterChoice;
				state.artifactId = artifactChoice;
				stateIndex = 2;
			}
		}
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

		if (info.inputState.enter == InputState::pressed)
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
