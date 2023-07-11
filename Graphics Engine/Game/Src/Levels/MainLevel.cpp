﻿#include "pch_game.h"
#include "Levels/MainLevel.h"
#include "Utils/Shuffle.h"
#include <Levels/LevelUtils.h>

#include "CardGame.h"
#include "Interpreters/TextInterpreter.h"
#include "States/InputState.h"
#include "States/GameState.h"
#include "States/BoardState.h"
#include "Utils/BoxCollision.h"

namespace game
{
	void MainLevel::Create(const LevelCreateInfo& info)
	{
		if (info.playerState.ironManMode)
			ClearSaveData();

		info.boardState = {};

		for (auto& flaw : info.gameState.flaws)
			flaw = -1;

		stage = Stage::bossReveal;
		switchingStage = true;
		depth = 0;
		chosenDiscoverOption = -1;

		currentBosses = jv::CreateArray<Boss>(info.arena, DISCOVER_LENGTH);
		currentRooms = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		currentMagics = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		currentFlaws = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);

		uint32_t count;

		GetDeck(nullptr, &count, info.bosses);
		bossDeck = jv::CreateVector<uint32_t>(info.arena, info.bosses.length);
		GetDeck(&bossDeck, nullptr, info.bosses);
		Shuffle(bossDeck.ptr, bossDeck.count);

		GetDeck(nullptr, &count, info.rooms);
		roomDeck = jv::CreateVector<uint32_t>(info.arena, count);

		GetDeck(nullptr, &count, info.magics);
		magicDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(&magicDeck, nullptr, info.magics);
		Shuffle(magicDeck.ptr, magicDeck.count);

		GetDeck(nullptr, &count, info.flaws);
		flawDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(&flawDeck, nullptr, info.flaws);
		Shuffle(flawDeck.ptr, flawDeck.count);

		info.monsterDeck.Clear();
		info.artifactDeck.Clear();
		GetDeck(&info.monsterDeck, nullptr, info.monsters);
		GetDeck(&info.artifactDeck, nullptr, info.artifacts);
		RemoveMonstersInParty(info.monsterDeck, info.playerState);
		RemoveArtifactsInParty(info.artifactDeck, info.playerState);
	}

	bool MainLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if(switchingStage)
		{
			switch (stage)
			{
			case Stage::bossReveal:
				SwitchToBossRevealStage(info, loadLevelIndex);
				break;
			case Stage::roomSelection:
				SwitchToRoomSelectionStage(info, loadLevelIndex);
				break;
			case Stage::receiveRewards:
				SwitchToRewardStage(info, loadLevelIndex);
				break;
			case Stage::exitFound:
				SwitchToExitFoundStage(info, loadLevelIndex);
				break;
			default:
				throw std::exception("Stage not supported!");
			}
			switchingStage = false;
		}
		switch (stage)
		{
		case Stage::bossReveal:
			UpdateBossRevealStage(info, loadLevelIndex);
			break;
		case Stage::roomSelection:
			UpdateRoomSelectionStage(info, loadLevelIndex);
			break;
		case Stage::receiveRewards:
			UpdateRewardStage(info, loadLevelIndex);
			break;
		case Stage::exitFound:
			UpdateExitFoundStage(info, loadLevelIndex);
			break;
		default:
			throw std::exception("Stage not supported!");
		}

		return true;
	}

	void MainLevel::SwitchToBossRevealStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			auto& boss = currentBosses[i];
			boss.id = bossDeck.Pop();
			boss.counters = 0;
		}
	}

	void MainLevel::UpdateBossRevealStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		Card* cards[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.bosses[currentBosses[i].id];

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = cards;
		renderInfo.length = DISCOVER_LENGTH;
		renderInfo.highlight = chosenDiscoverOption;
		RenderCards(renderInfo);

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "the bosses for this stage have been revealed.";
		textTask.lineLength = 20;
		textTask.position = glm::vec2(0, -.8f);
		textTask.scale = .06f;
		info.textTasks.Push(textTask);

		textTask.position.y *= -1;
		textTask.text = "press enter to continue.";
		info.textTasks.Push(textTask);

		if(info.inputState.enter == InputState::pressed)
		{
			stage = Stage::roomSelection;
			switchingStage = true;
		}
	}

	void MainLevel::SwitchToRoomSelectionStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		const bool addFlaw = depth % 10 == 4;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			jv::Vector<uint32_t>* decks[]
			{
				&roomDeck,
				&magicDeck,
				&flawDeck
			};

			for (auto& deck : decks)
			{
				if (deck->count > 0)
					continue;
				GetDeck(deck, nullptr, info.rooms);
				Shuffle(deck->ptr, deck->count);
				RemoveDuplicates(*deck, currentRooms.ptr, DISCOVER_LENGTH);
			}

			currentRooms[i] = roomDeck.Pop();
			currentMagics[i] = magicDeck.Pop();
			if(addFlaw)
				currentFlaws[i] = flawDeck.Pop();
		}

		chosenDiscoverOption = -1;
	}

	void MainLevel::UpdateRoomSelectionStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		// Render bosses.
		Card* cards[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.bosses[currentBosses[i].id];

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = cards;
		renderInfo.length = DISCOVER_LENGTH;
		renderInfo.center = glm::vec2(0, -CARD_HEIGHT);
		renderInfo.highlight = chosenDiscoverOption;
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;

		const uint32_t selected = RenderCards(renderInfo);

		if (info.inputState.lMouse == InputState::pressed)
			chosenDiscoverOption = selected == chosenDiscoverOption ? -1 : selected;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			uint32_t counters = currentBosses[i].counters;
			counters += i == chosenDiscoverOption;

			TextTask textTask{};
			textTask.center = true;
			textTask.text = TextInterpreter::IntToConstCharPtr(counters, info.frameArena);
			textTask.position = glm::vec2(-CARD_WIDTH_OFFSET * DISCOVER_LENGTH / 2 + CARD_WIDTH_OFFSET * 2 * i - CARD_WIDTH_OFFSET, -CARD_HEIGHT);
			textTask.scale = .06f;
			info.textTasks.Push(textTask);
		}

		// Render flaws.
		if(depth % 10 == 4)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.flaws[currentFlaws[i]];
			renderInfo.center.x += CARD_WIDTH * 2;
			RenderCards(renderInfo);
		}

		// Render rooms.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.rooms[currentRooms[i]];
		
		renderInfo.center = glm::vec2(0, CARD_HEIGHT);
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
		RenderCards(renderInfo);

		// Render magics.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.magics[currentMagics[i]];
		
		renderInfo.center.x += CARD_WIDTH * 2;
		RenderCards(renderInfo);

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "select the road to take.";
		textTask.lineLength = 20;
		textTask.position = glm::vec2(0, -.8f);
		textTask.scale = .06f;
		info.textTasks.Push(textTask);

		if(chosenDiscoverOption != -1)
		{
			textTask.position.y *= -1;
			textTask.text = "press enter to continue.";
			info.textTasks.Push(textTask);

			if (info.inputState.enter == InputState::pressed)
			{
				chosenRoom = chosenDiscoverOption;
				auto& counters = currentBosses[chosenDiscoverOption].counters;
				++counters;
				++depth;
				
				stage = Stage::receiveRewards;
				switchingStage = true;
				chosenDiscoverOption = -1;
			}
		}
	}

	void MainLevel::SwitchToRewardStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		chosenDiscoverOption = -1;
		scroll = 0;
		rewardedMagicCard = false;
	}

	void MainLevel::UpdateRewardStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_CAPACITY]{};

		if(!rewardedMagicCard)
		{
			for (uint32_t i = 0; i < MAGIC_CAPACITY; ++i)
				cards[i] = &info.magics[info.gameState.magics[i]];

			scroll += info.inputState.scroll * .1f;

			RenderCardInfo renderInfo{};
			renderInfo.levelUpdateInfo = &info;
			renderInfo.length = MAGIC_CAPACITY;
			renderInfo.highlight = chosenDiscoverOption;
			renderInfo.cards = cards;
			renderInfo.center.x = scroll;
			renderInfo.center.y = CARD_HEIGHT;
			renderInfo.additionalSpacing = -CARD_SPACING;
			renderInfo.lineLength = MAGIC_CAPACITY / 2;
			const uint32_t choice = RenderCards(renderInfo);

			cards[0] = &info.magics[currentMagics[chosenRoom]];
			renderInfo.length = 1;
			renderInfo.highlight = -1;
			renderInfo.center.y *= -1;
			renderInfo.center.x = 0;
			RenderCards(renderInfo);

			if (info.inputState.lMouse == InputState::pressed)
				chosenDiscoverOption = choice == chosenDiscoverOption ? -1 : choice;

			TextTask textTask{};
			textTask.center = true;
			textTask.lineLength = 20;
			textTask.scale = .06f;
			textTask.position = glm::vec2(0, .8f);
			textTask.text = "press enter to continue.";
			info.textTasks.Push(textTask);

			textTask.text = "select card to replace, if any.";
			textTask.position.y *= -1;
			info.textTasks.Push(textTask);

			if (info.inputState.enter == InputState::pressed)
			{
				if (chosenDiscoverOption != -1)
				{
					magicDeck.Add() = info.gameState.magics[chosenDiscoverOption];
					info.gameState.magics[chosenDiscoverOption] = currentMagics[chosenRoom];
				}

				rewardedMagicCard = true;
			}
			return;
		}

		if(depth % 10 == 5)
		{
			auto& playerState = info.playerState;
			auto& gameState = info.gameState;

			bool selected[PARTY_ACTIVE_CAPACITY];
			bool flawSlotAvailable = false;

			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				auto& flaw = info.gameState.flaws[i];
				selected[i] = flaw == -1;
				flawSlotAvailable = flawSlotAvailable ? true : flaw == -1;
			}

			if(flawSlotAvailable)
			{
				TextTask textTask{};
				textTask.center = true;
				textTask.lineLength = 20;
				textTask.scale = .06f;
				textTask.position = glm::vec2(0, -.8f);
				textTask.text = "select one ally to carry this flaw.";
				info.textTasks.Push(textTask);

				for (uint32_t i = 0; i < gameState.partySize; ++i)
					cards[i] = &info.monsters[playerState.monsterIds[gameState.partyMembers[i]]];

				RenderCardInfo renderInfo{};
				renderInfo.levelUpdateInfo = &info;
				renderInfo.length = gameState.partySize;
				renderInfo.cards = cards;
				renderInfo.selectedArr = selected;
				renderInfo.center.y = CARD_HEIGHT;
				renderInfo.additionalSpacing = -CARD_SPACING;
				const uint32_t choice = RenderCards(renderInfo);

				cards[0] = &info.flaws[currentFlaws[chosenRoom]];
				renderInfo.center.y *= -1;
				renderInfo.length = 1;
				renderInfo.selectedArr = nullptr;
				RenderCards(renderInfo);

				bool validChoice = false;
				if (info.inputState.lMouse == InputState::pressed)
					if (choice != -1 && selected[choice])
					{
						info.gameState.flaws[choice] = currentFlaws[chosenRoom];
						validChoice = true;
					}

				if (!validChoice)
					return;
			}
		}

		if (depth % 5 == 0)
			stage = Stage::exitFound;
		else
			stage = Stage::roomSelection;
		switchingStage = true;
	}

	void MainLevel::SwitchToExitFoundStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		TextTask textTask{};
		textTask.center = true;
		textTask.lineLength = 20;
		textTask.scale = .06f;
		textTask.position = glm::vec2(0, -.8f);
		textTask.text = "an exit leading outside has been found.";
		info.textTasks.Push(textTask);
	}

	void MainLevel::UpdateExitFoundStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = -.18;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.scale.x = 1;
		buttonRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
		info.renderTasks.Push(buttonRenderTask);

		if (info.inputState.lMouse == InputState::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
			{
				if (depth % 10 == 0)
					stage = Stage::bossReveal;
				else
					stage = Stage::roomSelection;
				switchingStage = true;
				return;
			}

		TextTask buttonTextTask{};
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "continue forward";
		buttonTextTask.scale = .06f;
		info.textTasks.Push(buttonTextTask);

		buttonRenderTask.position.y *= -1;
		info.renderTasks.Push(buttonRenderTask);

		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "save and escape dungeon";
		info.textTasks.Push(buttonTextTask);

		if (info.inputState.lMouse == InputState::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
			{
				SaveData(info.playerState);
				loadLevelIndex = LevelIndex::mainMenu;
			}
	}
}
