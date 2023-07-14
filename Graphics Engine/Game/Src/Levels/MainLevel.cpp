#include "pch_game.h"
#include "Levels/MainLevel.h"
#include "Utils/Shuffle.h"
#include <Levels/LevelUtils.h>

#include "CardGame.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Math.h"
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
		currentArtifacts = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);

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
		const bool addFlaw = depth % 10 == ROOM_COUNT_BEFORE_FLAW - 1;
		const bool addArtifact = depth % 10 == ROOM_COUNT_BEFORE_BOSS -1;

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

			if(info.artifactDeck.count == 0)
			{
				GetDeck(&info.artifactDeck, nullptr, info.rooms);
				Shuffle(info.artifactDeck.ptr, info.artifactDeck.count);
				RemoveArtifactsInParty(info.artifactDeck, info.playerState);
			}

			currentRooms[i] = roomDeck.Pop();
			currentMagics[i] = magicDeck.Pop();
			if (addFlaw)
				currentFlaws[i] = flawDeck.Pop();
			else if (addArtifact)
				currentArtifacts[i] = info.artifactDeck.Pop();
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

		uint32_t selected = RenderCards(renderInfo);
		
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
		if(depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.flaws[currentFlaws[i]];
			renderInfo.center.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}
		// Render artifacts.
		else if (depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.artifacts[currentArtifacts[i]];
			renderInfo.center.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}

		// Render rooms.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.rooms[currentRooms[i]];
		
		renderInfo.center = glm::vec2(0, CARD_HEIGHT);
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
		auto choice = RenderCards(renderInfo);
		selected = choice != -1 ? choice : selected;

		// Render magics.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.magics[currentMagics[i]];
		
		renderInfo.center.x += CARD_WIDTH * 2;
		choice = RenderCards(renderInfo);
		selected = choice != -1 ? choice : selected;

		if (info.inputState.lMouse == InputState::pressed)
			chosenDiscoverOption = selected == chosenDiscoverOption ? -1 : selected;

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

		if(depth % ROOM_COUNT_BEFORE_BOSS == 0)
			for (auto& slotCount : info.playerState.artifactSlotCounts)
				slotCount = jv::Max(slotCount, depth / ROOM_COUNT_BEFORE_BOSS);
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

		if(depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW)
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
				renderInfo.highlight = chosenDiscoverOption;
				renderInfo.center.y = CARD_HEIGHT;
				renderInfo.additionalSpacing = -CARD_SPACING;
				const uint32_t choice = RenderCards(renderInfo);

				cards[0] = &info.flaws[currentFlaws[chosenRoom]];
				renderInfo.center.y *= -1;
				renderInfo.length = 1;
				renderInfo.selectedArr = nullptr;
				renderInfo.highlight = -1;
				RenderCards(renderInfo);

				if (info.inputState.lMouse == InputState::pressed)
					if (choice == -1 || selected[choice])
						chosenDiscoverOption = choice == chosenDiscoverOption ? -1 : choice;

				bool validChoice = false;
				if(chosenDiscoverOption != -1)
				{
					textTask.position.y *= -1;
					textTask.text = "press enter to continue.";
					info.textTasks.Push(textTask);

					if (info.inputState.enter == InputState::pressed)
					{
						info.gameState.flaws[chosenDiscoverOption] = currentFlaws[chosenRoom];
						validChoice = true;
					}
				}

				if (!validChoice)
					return;
			}
		}
		else if(depth % ROOM_COUNT_BEFORE_BOSS == 0)
		{
			auto& playerState = info.playerState;
			auto& gameState = info.gameState;

			TextTask textTask{};
			textTask.center = true;
			textTask.lineLength = 20;
			textTask.scale = .06f;
			textTask.position = glm::vec2(0, -.8f);
			textTask.text = "you can switch your artifacts around. press enter to continue.";
			info.textTasks.Push(textTask);

			for (uint32_t i = 0; i < gameState.partySize; ++i)
				cards[i] = &info.monsters[playerState.monsterIds[gameState.partyMembers[i]]];

			RenderCardInfo renderInfo{};
			renderInfo.levelUpdateInfo = &info;
			renderInfo.length = gameState.partySize;
			renderInfo.cards = cards;
			renderInfo.center.y = CARD_HEIGHT;
			renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
			RenderCards(renderInfo);

			const uint32_t currentArtifactIndex = currentArtifacts[chosenRoom];
			cards[0] = currentArtifactIndex == -1 ? nullptr : &info.artifacts[currentArtifactIndex];
			renderInfo.center.y *= -1;
			renderInfo.length = 1;
			renderInfo.additionalSpacing = -CARD_SPACING;
			RenderCards(renderInfo);

			renderInfo.center.x = -static_cast<float>(gameState.partySize - 1) * CARD_WIDTH_OFFSET;
			renderInfo.center.y *= -1;
			renderInfo.center.y += CARD_HEIGHT * 2;
			renderInfo.length = MONSTER_ARTIFACT_CAPACITY;
			renderInfo.lineLength = MONSTER_ARTIFACT_CAPACITY / 2;

			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const uint32_t partyMember = gameState.partyMembers[i];
				const uint32_t unlockedCount = playerState.artifactSlotCounts[partyMember];
				const uint32_t artifactStartIndex = partyMember * MONSTER_ARTIFACT_CAPACITY;

				for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
				{
					const uint32_t index = playerState.artifacts[j + artifactStartIndex];
					cards[j] = index == -1 ? nullptr : &info.artifacts[index];
				}

				bool unlocked[MONSTER_ARTIFACT_CAPACITY];
				for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
					unlocked[j] = unlockedCount > j;

				renderInfo.selectedArr = unlocked;
				const uint32_t choice = RenderCards(renderInfo);
				if(info.inputState.lMouse == InputState::pressed && choice != -1 && choice < unlockedCount)
				{
					const uint32_t swappable = currentArtifacts[chosenRoom];
					currentArtifacts[chosenRoom] = playerState.artifacts[artifactStartIndex + choice];
					playerState.artifacts[artifactStartIndex + choice] = swappable;
				}

				renderInfo.center.x += CARD_WIDTH_OFFSET * 2;
			}

			if (info.inputState.enter != InputState::pressed)
				return;
		}

		if (depth % ROOM_COUNT_BEFORE_EXIT == 0)
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
				if (depth % ROOM_COUNT_BEFORE_BOSS == 0)
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
