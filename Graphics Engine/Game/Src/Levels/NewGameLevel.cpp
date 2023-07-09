#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "Levels/LevelUtils.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/Shuffle.h"

namespace game
{
	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		monsterChoice = -1;
		artifactChoice = -1;
		confirmedChoices = false;

		ClearSaveData();
		GetDeck(info.monsterDeck, info.monsters, info.playerState, ValidateMonsterInclusion);
		GetDeck(info.artifactDeck, info.artifacts, info.playerState, ValidateArtifactInclusion);

		// Create a discover option for your initial monster.
		monsterDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(info.monsterDeck.ptr, info.monsterDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			monsterDiscoverOptions[i] = info.monsterDeck.Pop();
		// Create a discover option for your initial artifact.
		artifactDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(info.artifactDeck.ptr, info.artifactDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			artifactDiscoverOptions[i] = info.artifactDeck.Pop();
	}

	bool NewGameLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!confirmedChoices)
		{
			Card* cards[DISCOVER_LENGTH]{};

			RenderCardInfo renderInfo{};
			renderInfo.levelUpdateInfo = &info;
			renderInfo.cards = cards;
			renderInfo.length = DISCOVER_LENGTH;
			renderInfo.center = glm::vec2(0, -.3);
			renderInfo.highlight = monsterChoice;

			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.monsters[monsterDiscoverOptions[i]];
			auto choice = RenderCards(renderInfo);
			if (info.inputState.lMouse == InputState::pressed && choice != -1)
				monsterChoice = choice == monsterChoice ? -1 : choice;

			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.artifacts[artifactDiscoverOptions[i]];
			renderInfo.center.y *= -1;
			renderInfo.highlight = artifactChoice;
			choice = RenderCards(renderInfo);
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

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = &cards;
		renderInfo.length = 1;
		RenderCards(renderInfo);

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
