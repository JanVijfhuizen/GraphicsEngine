#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "Levels/LevelUtils.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/Shuffle.h"

namespace game
{
	template <typename T>
	void GetDeck(jv::Vector<uint32_t>& outDeck, const jv::Array<T>& cards, const PlayerState& playerState,
		bool(*func)(uint32_t, const PlayerState&))
	{
		outDeck.Clear();
		for (uint32_t i = 0; i < outDeck.length; ++i)
		{
			if (cards[i].unique)
				continue;
			if (!func(i, playerState))
				continue;
			outDeck.Add() = i;
		}
	}

	bool ValidateMonsterInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
			if (playerState.monsterIds[j] == id)
				return false;
		return true;
	}

	bool ValidateArtifactInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
		{
			const uint32_t artifactCount = playerState.artifactsCounts[j];
			for (uint32_t k = 0; k < artifactCount; ++k)
				if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					return false;
		}
		return true;
	}

	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		ClearSaveData();
		GetDeck(info.monsterDeck, info.monsters, info.playerState, ValidateMonsterInclusion);
		GetDeck(info.artifactDeck, info.artifacts, info.playerState, ValidateArtifactInclusion);

		// Create a discover option for your initial monster.
		monsterDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(info.monsterDeck.ptr, info.monsterDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			monsterDiscoverOptions[i] = info.monsterDeck.Pop();
		// Create a discover option for your initial artifact.
		artifactDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(info.artifactDeck.ptr, info.artifactDeck.length);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			artifactDiscoverOptions[i] = info.artifactDeck.Pop();
	}

	bool NewGameLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!confirmedChoices)
		{
			Card* cards[DISCOVER_LENGTH]{};

			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.monsters[monsterDiscoverOptions[i]];
			auto choice = RenderCards(info, cards, DISCOVER_LENGTH, glm::vec2(0, -.3), monsterChoice);
			if (info.inputState.lMouse == InputState::pressed && choice != -1)
				monsterChoice = choice == monsterChoice ? -1 : choice;

			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.artifacts[artifactDiscoverOptions[i]];
			choice = RenderCards(info, cards, DISCOVER_LENGTH, glm::vec2(0, .3), artifactChoice);
			if (info.inputState.lMouse == InputState::pressed && choice != -1)
				artifactChoice = choice == artifactChoice ? -1 : choice;

			TextTask buttonTextTask{};
			buttonTextTask.center = true;
			buttonTextTask.text = "choose your starting cards.";
			buttonTextTask.position = glm::vec2(0, -.8f);
			buttonTextTask.scale = .06f;
			info.textTasks.Push(buttonTextTask);

			if (monsterChoice != -1 && artifactChoice != -1)
			{
				TextTask textTask{};
				textTask.center = true;
				textTask.text = "press enter to confirm your choice.";
				textTask.position = glm::vec2(0, .8f);
				textTask.scale = .06f;
				info.textTasks.Push(textTask);

				if (info.inputState.enter == InputState::pressed)
					confirmedChoices = true;
			}
			return true;
		}

		TextTask joinTextTask{};
		joinTextTask.center = true;
		joinTextTask.text = "daisy joins you on your adventure.";
		joinTextTask.position = glm::vec2(0, -.8f);
		joinTextTask.scale = .06f;
		info.textTasks.Push(joinTextTask);

		Card* cards = &info.monsters[0];
		RenderCards(info, &cards, 1, glm::vec2(0));

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "press enter to continue.";
		textTask.scale = .06f;
		textTask.position = glm::vec2(0, .8f);
		info.textTasks.Push(textTask);

		if (info.inputState.enter == InputState::pressed)
		{
			auto& playerState = info.playerState;
			playerState.monsterIds[0] = monsterDiscoverOptions[monsterChoice];
			playerState.artifactsCounts[0] = 1;
			playerState.artifacts[0] = artifactDiscoverOptions[artifactChoice];
			playerState.monsterIds[1] = 0;
			playerState.partySize = 2;
			SaveData(playerState);
			loadLevelIndex = LevelIndex::main;
		}

		return true;
	}
}
