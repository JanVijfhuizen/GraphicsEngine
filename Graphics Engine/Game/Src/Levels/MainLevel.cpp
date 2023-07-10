#include "pch_game.h"
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
		info.gameState = {};
		info.boardState = {};

		stage = Stage::bossReveal;
		switchingStage = true;
		depth = 0;
		chosenDiscoverOption = -1;

		currentBosses = jv::CreateArray<Boss>(info.arena, DISCOVER_LENGTH);
		currentRooms = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		currentMagics = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);

		uint32_t count;

		GetDeck(nullptr, &count, info.bosses, info.playerState);
		bossDeck = jv::CreateVector<uint32_t>(info.arena, info.bosses.length);
		GetDeck(&bossDeck, nullptr, info.bosses, info.playerState);
		Shuffle(bossDeck.ptr, bossDeck.count);

		GetDeck(nullptr, &count, info.rooms, info.playerState);
		roomDeck = jv::CreateVector<uint32_t>(info.arena, count);

		GetDeck(nullptr, &count, info.magics, info.playerState);
		magicDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(&magicDeck, nullptr, info.magics, info.playerState);
		Shuffle(magicDeck.ptr, magicDeck.count);

		info.monsterDeck.Clear();
		info.artifactDeck.Clear();
		GetDeck(&info.monsterDeck, nullptr, info.monsters, info.playerState);
		GetDeck(&info.artifactDeck, nullptr, info.artifacts, info.playerState);
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
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			if (roomDeck.count == 0)
			{
				GetDeck(&roomDeck, nullptr, info.rooms, info.playerState);
				Shuffle(roomDeck.ptr, roomDeck.count);

				// If the room is already in play, remove it from the shuffled deck.
				for (uint32_t j = 0; j < i; ++j)
				{
					bool removed = false;

					for (int32_t k = static_cast<int32_t>(roomDeck.count) - 1; k >= 0; --k)
					{
						if (currentRooms[j] == roomDeck[k])
						{
							roomDeck.RemoveAt(k);
							removed = true;
							break;
						}
					}
					if (removed)
						break;
				}
			}

			currentRooms[i] = roomDeck.Pop();
			currentMagics[i] = magicDeck.Pop();
		}
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
			textTask.position = glm::vec2(-CARD_WIDTH_OFFSET * DISCOVER_LENGTH / 2 + CARD_WIDTH_OFFSET * 2 * i, -CARD_HEIGHT);
			textTask.scale = .06f;
			info.textTasks.Push(textTask);
		}

		// Render rooms and magics.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.rooms[currentRooms[i]];
		
		renderInfo.center = glm::vec2(0, CARD_HEIGHT);
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
		RenderCards(renderInfo);

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

				for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
					if(chosenDiscoverOption != i)
						magicDeck.Add() = currentMagics[i];
				
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
	}

	void MainLevel::UpdateRewardStage(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_CAPACITY]{};
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
			if(chosenDiscoverOption != -1)
			{
				magicDeck.Add() = info.gameState.magics[chosenDiscoverOption];
				info.gameState.magics[chosenDiscoverOption] = currentMagics[chosenRoom];
			}

			if (depth % 5 == 0)
				stage = Stage::exitFound;
			else
				stage = Stage::roomSelection;
			
			switchingStage = true;
		}
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
