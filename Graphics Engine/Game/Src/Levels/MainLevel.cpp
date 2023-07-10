#include "pch_game.h"
#include "Levels/MainLevel.h"
#include "Utils/Shuffle.h"
#include <Levels/LevelUtils.h>

#include "Interpreters/TextInterpreter.h"
#include "States/InputState.h"
#include "States/GameState.h"
#include "States/BoardState.h"

namespace game
{
	bool EmptyValidation(const uint32_t id, const PlayerState& playerState)
	{
		return true;
	}

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

		bossDeck = jv::CreateVector<uint32_t>(info.arena, info.bosses.length);
		GetDeck(bossDeck, info.bosses, info.playerState, EmptyValidation);
		Shuffle(bossDeck.ptr, bossDeck.count);
		roomDeck = jv::CreateVector<uint32_t>(info.arena, info.rooms.length);
		magicDeck = jv::CreateVector<uint32_t>(info.arena, info.magics.length);
		GetDeck(magicDeck, info.magics, info.playerState, EmptyValidation);
		Shuffle(magicDeck.ptr, magicDeck.count);
		
		GetDeck(info.monsterDeck, info.monsters, info.playerState, ValidateMonsterInclusion);
		GetDeck(info.artifactDeck, info.artifacts, info.playerState, ValidateArtifactInclusion);
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
				GetDeck(roomDeck, info.rooms, info.playerState, EmptyValidation);
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

		scroll += info.inputState.scroll;

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

			if (depth == 10)
			{
				depth = 0;
				stage = Stage::bossReveal;
			}
			else
				stage = Stage::roomSelection;

			switchingStage = true;
		}
	}
}
