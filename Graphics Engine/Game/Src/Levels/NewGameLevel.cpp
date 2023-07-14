#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "Levels/LevelUtils.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game
{
	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		monsterChoice = -1;
		artifactChoice = -1;
		confirmedMode = false;
		confirmedChoices = false;

		ClearSaveData();

		GetDeck(&info.monsterDeck, nullptr, info.monsters);
		GetDeck(&info.artifactDeck, nullptr, info.artifacts);

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
		if(!confirmedMode)
		{
			TextTask textTask{};
			textTask.center = true;
			textTask.text = "choose your mode.";
			textTask.position = glm::vec2(0, -.8f);
			textTask.scale = .06f;
			info.textTasks.Push(textTask);

			RenderTask buttonRenderTask{};
			buttonRenderTask.position.y = -.18;
			buttonRenderTask.scale.y *= .12f;
			buttonRenderTask.scale.x = .4f;
			buttonRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
			info.renderTasks.Push(buttonRenderTask);

			if (info.inputState.lMouse == InputState::pressed)
				if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
				{
					info.playerState.ironManMode = false;
					confirmedMode = true;
				}

			TextTask buttonTextTask{};
			buttonTextTask.center = true;
			buttonTextTask.position = buttonRenderTask.position;
			buttonTextTask.text = "standard";
			buttonTextTask.scale = .06f;
			info.textTasks.Push(buttonTextTask);

			buttonRenderTask.position.y *= -1;
			info.renderTasks.Push(buttonRenderTask);
			if (info.inputState.lMouse == InputState::pressed)
				if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
				{
					info.playerState.ironManMode = true;
					confirmedMode = true;
				}

			buttonTextTask.position = buttonRenderTask.position;
			buttonTextTask.text = "iron man";
			info.textTasks.Push(buttonTextTask);

			return true;
		}

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
			playerState.artifactSlotCounts[0] = 1;
			playerState.artifacts[0] = artifactDiscoverOptions[artifactChoice];
			playerState.monsterIds[1] = 0;
			playerState.partySize = 2;
			SaveData(playerState);
			loadLevelIndex = LevelIndex::partySelect;
		}

		return true;
	}
}
