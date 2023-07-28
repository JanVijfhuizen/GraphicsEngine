#include "pch_game.h"
#include "Levels/MainLevel.h"
#include <Levels/LevelUtils.h>

#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Math.h"
#include "States/InputState.h"
#include "States/GameState.h"
#include "States/BoardState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game
{
	MainLevel::State::Decks MainLevel::State::Decks::Create(const LevelCreateInfo& info)
	{
		Decks decks{};
		uint32_t count;

		GetDeck(nullptr, &count, info.bosses);
		decks.bosses = jv::CreateVector<uint32_t>(info.arena, info.bosses.length);
		GetDeck(nullptr, &count, info.rooms);
		decks.rooms = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.magics);
		decks.magics = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.flaws);
		decks.flaws = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.events);
		decks.events = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.monsters);
		decks.monsters = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.artifacts);
		decks.artifacts = jv::CreateVector<uint32_t>(info.arena, count);

		return decks;
	}

	void MainLevel::State::RemoveDuplicates(const LevelInfo& info, jv::Vector<uint32_t>& deck, uint32_t Path::* mem) const
	{
		for (const auto& path : paths)
			for (uint32_t i = 0; i < deck.count; ++i)
				if (deck[i] == path.*mem)
				{
					deck.RemoveAt(i);
					break;
				}
	}

	uint32_t MainLevel::State::GetMonster(const LevelInfo& info, const BoardState& boardState)
	{
		auto& monsters = decks.monsters;
		if (monsters.count == 0)
		{
			GetDeck(&monsters, nullptr, info.monsters);

			for (int32_t i = static_cast<int32_t>(monsters.count) - 1; i >= 0; --i)
			{
				bool removed = false;

				for (uint32_t j = 0; j < boardState.alliedMonsterCount; ++j)
					if (monsters[i] == boardState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < boardState.enemyMonsterCount; ++j)
					if (monsters[i] == boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < info.playerState.partySize; ++j)
					if(monsters[i] == info.playerState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						break;
					}
			}
		}
		return monsters.Pop();
	}

	uint32_t MainLevel::State::GetBoss(const LevelInfo& info)
	{
		auto& bosses = decks.bosses;
		if (bosses.count == 0)
		{
			GetDeck(&bosses, nullptr, info.bosses);
			RemoveDuplicates(info, bosses, &Path::boss);
		}
		return bosses.Pop();
	}

	uint32_t MainLevel::State::GetRoom(const LevelInfo& info)
	{
		auto& rooms = decks.rooms;
		if (rooms.count == 0)
		{
			GetDeck(&rooms, nullptr, info.rooms);
			RemoveDuplicates(info, rooms, &Path::room);
		}
		return rooms.Pop();
	}

	uint32_t MainLevel::State::GetMagic(const LevelInfo& info)
	{
		auto& magics = decks.magics;
		if (magics.count == 0)
		{
			GetDeck(&magics, nullptr, info.magics);
			RemoveDuplicates(info, magics, &Path::magic);
		}
		RemoveMagicsInParty(magics, info.gameState);
		return magics.Pop();
	}

	uint32_t MainLevel::State::GetArtifact(const LevelInfo& info)
	{
		auto& artifacts = decks.artifacts;
		if (artifacts.count == 0)
		{
			GetDeck(&artifacts, nullptr, info.artifacts);
			RemoveDuplicates(info, artifacts, &Path::artifact);
		}
		RemoveArtifactsInParty(artifacts, info.playerState);
		return artifacts.Pop();
	}

	uint32_t MainLevel::State::GetFlaw(const LevelInfo& info)
	{
		auto& flaws = decks.flaws;
		if (flaws.count == 0)
		{
			GetDeck(&flaws, nullptr, info.flaws);
			RemoveDuplicates(info, flaws, &Path::flaw);
		}
		RemoveFlawsInParty(flaws, info.gameState);
		return flaws.Pop();
	}

	uint32_t MainLevel::State::GetEvent(const LevelInfo& info)
	{
		auto& events = decks.events;
		if (events.count == 0)
			GetDeck(&events, nullptr, info.flaws);
		return events.Pop();
	}

	uint32_t MainLevel::State::Draw(const LevelInfo& info)
	{
		if(magicDeck.count == 0)
		{
			for (const auto& magic : info.gameState.magics)
				magicDeck.Add() = magic;
			Shuffle(magicDeck.ptr, magicDeck.count);
		}

		return magicDeck.Pop();
	}

	MainLevel::State MainLevel::State::Create(const LevelCreateInfo& info)
	{
		State state{};
		state.decks = Decks::Create(info);
		state.paths = jv::CreateArray<Path>(info.arena, DISCOVER_LENGTH);
		state.magicDeck = jv::CreateVector<uint32_t>(info.arena, MAGIC_DECK_SIZE);
		state.hand = jv::CreateVector<uint32_t>(info.arena, HAND_DRAW_COUNT);
		return state;
	}

	void MainLevel::BossRevealState::Reset(State& state, const LevelInfo& info)
	{
		for (auto& path : state.paths)
		{
			path = {};
			path.boss = state.GetBoss(info);
		}
	}

	bool MainLevel::BossRevealState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.bosses[state.paths[i].boss];

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = cards;
		renderInfo.length = DISCOVER_LENGTH;
		RenderCards(renderInfo);

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "the bosses for this stage have been revealed.";
		textTask.lineLength = 20;
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(textTask);

		textTask.position = TEXT_CENTER_BOT_POSITION;
		textTask.text = "press enter to continue.";
		info.textTasks.Push(textTask);

		if (info.inputState.enter == InputState::pressed)
			stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		return true;
	}

	void MainLevel::PathSelectState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;

		const bool addFlaw = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1;
		const bool addArtifact = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1;

		for (auto& path : state.paths)
		{
			path.room = state.GetRoom(info);
			path.magic = state.GetMagic(info);
			if (addFlaw)
				path.flaw = state.GetFlaw(info);
			if (addArtifact)
				path.artifact = state.GetArtifact(info);
		}
	}

	bool MainLevel::PathSelectState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		// Render bosses.
		Card* cards[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.bosses[state.paths[i].boss];

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.cards = cards;
		renderInfo.length = DISCOVER_LENGTH;
		renderInfo.position = glm::vec2(0, -CARD_HEIGHT);
		renderInfo.highlight = discoverOption;
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;

		uint32_t selected = RenderCards(renderInfo);

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			uint32_t counters = state.paths[i].counters;
			counters += i == discoverOption;

			TextTask textTask{};
			textTask.center = true;
			textTask.text = TextInterpreter::IntToConstCharPtr(counters, info.frameArena);
			textTask.position = glm::vec2(-CARD_WIDTH_OFFSET * DISCOVER_LENGTH / 2 + CARD_WIDTH_OFFSET * 2 * i - CARD_WIDTH_OFFSET, -CARD_HEIGHT);
			textTask.scale = TEXT_BIG_SCALE;
			info.textTasks.Push(textTask);
		}

		// Render flaws.
		if (state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.flaws[state.paths[i].flaw];
			renderInfo.position.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}
		// Render artifacts.
		else if (state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.artifacts[state.paths[i].artifact];
			renderInfo.position.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}

		// Render rooms.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.rooms[state.paths[i].room];

		renderInfo.position = glm::vec2(0, CARD_HEIGHT);
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
		auto choice = RenderCards(renderInfo);
		selected = choice != -1 ? choice : selected;

		// Render magics.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.magics[state.paths[i].magic];

		renderInfo.position.x += CARD_WIDTH * 2;
		choice = RenderMagicCards(info.frameArena, renderInfo);
		selected = choice != -1 ? choice : selected;

		if (info.inputState.lMouse == InputState::pressed)
			discoverOption = selected == discoverOption ? -1 : selected;

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "select the road to take.";
		textTask.lineLength = 20;
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(textTask);

		if (discoverOption != -1)
		{
			textTask.position = TEXT_CENTER_BOT_POSITION;
			textTask.text = "press enter to continue.";
			info.textTasks.Push(textTask);

			if (info.inputState.enter == InputState::pressed)
			{
				state.chosenPath = discoverOption;
				auto& path = state.paths[discoverOption];
				++path.counters;
				++state.depth;

				stateIndex = static_cast<uint32_t>(StateNames::combat);
			}
		}

		return true;
	}

	void MainLevel::CombatState::Reset(State& state, const LevelInfo& info)
	{
		boardState = {};
		allySelected = -1;
		newTurn = true;
		recruitableMonster = -1;

		boardState.AddParty(info);

		const uint32_t enemyCount = MONSTER_CAPACITIES[jv::Min((state.depth - 1) / ROOM_COUNT_BEFORE_BOSS, TOTAL_BOSS_COUNT - 1)];
		for (uint32_t i = 0; i < enemyCount; ++i)
			boardState.TryAddEnemy(info, state.GetMonster(info, boardState));
	}

	bool MainLevel::CombatState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		if(recruitableMonster != -1)
		{
			if(boardState.partySize < PARTY_ACTIVE_CAPACITY && info.playerState.partySize < PARTY_CAPACITY)
			{
				TextTask textTask{};
				textTask.center = true;
				textTask.text = "you may recruit the last monster defeated.";
				textTask.lineLength = 20;
				textTask.position = TEXT_CENTER_TOP_POSITION;
				textTask.scale = TEXT_BIG_SCALE;
				info.textTasks.Push(textTask);

				Card* card = &info.monsters[recruitableMonster];
				RenderMonsterCardInfo renderInfo{};
				renderInfo.levelUpdateInfo = &info;
				renderInfo.cards = &card;
				renderInfo.length = 1;
				renderInfo.position.y -= .25f;
				RenderMonsterCards(info.frameArena, renderInfo);

				RenderTask buttonRenderTask{};
				buttonRenderTask.position.y = .5f;
				buttonRenderTask.scale.y *= .12f;
				buttonRenderTask.scale.x = .4f;
				buttonRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::fallback)].subTexture;
				info.renderTasks.Push(buttonRenderTask);

				TextTask buttonTextTask{};
				buttonTextTask.center = true;
				buttonTextTask.position = buttonRenderTask.position;
				buttonTextTask.text = "recruit";
				buttonTextTask.scale = .06f;
				info.textTasks.Push(buttonTextTask);

				bool decided = false;
				bool joins = false;

				if (info.inputState.lMouse == InputState::pressed)
					if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
					{
						joins = true;
						decided = true;
					}

				buttonRenderTask.position.y -= BUTTON_Y_SCALE * 2 + BUTTON_Y_OFFSET;
				info.renderTasks.Push(buttonRenderTask);

				buttonTextTask.position.y -= BUTTON_Y_SCALE * 2 + BUTTON_Y_OFFSET;
				buttonTextTask.text = "pass";
				info.textTasks.Push(buttonTextTask);

				auto& gameState = info.gameState;

				if (info.inputState.lMouse == InputState::pressed)
					if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
						decided = true;

				if(!decided)
					return true;

				if(joins)
				{
					info.playerState.AddMonster(recruitableMonster);
					gameState.healths[gameState.partySize] = info.monsters[recruitableMonster].health;
					gameState.partyIds[gameState.partySize++] = info.playerState.partySize - 1;
				}
			}

			stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);
			return true;
		}

		if(newTurn)
		{
			for (auto& b : tapped)
				b = false;
			newTurn = false;
			for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
				boardState.RerollEnemyTarget(i);
			eventCard = state.GetEvent(info);

			state.hand.Clear();
			for (uint32_t i = 0; i < HAND_DRAW_COUNT; ++i)
				state.hand.Add() = state.Draw(info);
		}

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "combat phase.";
		textTask.lineLength = 20;
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(textTask);

		Card* cards[BOARD_CAPACITY_PER_SIDE > HAND_MAX_SIZE ? BOARD_CAPACITY_PER_SIDE : HAND_MAX_SIZE]{};

		for (uint32_t i = 0; i < state.hand.count; ++i)
			cards[i] = &info.magics[state.hand[i]];

		RenderCardInfo handRenderInfo{};
		handRenderInfo.levelUpdateInfo = &info;
		handRenderInfo.cards = cards;
		handRenderInfo.length = state.hand.count;
		handRenderInfo.position = glm::vec2(0, 1);
		handRenderInfo.additionalSpacing = -CARD_SPACING / 2;
		handRenderInfo.state = RenderCardInfo::State::summary;
		handRenderInfo.cardHeightPctIncreaseOnHovered = 1;
		handRenderInfo.priority = true;
		RenderCards(handRenderInfo);

		RenderCardInfo roomRenderInfo{};
		roomRenderInfo.levelUpdateInfo = &info;
		roomRenderInfo.cards = cards;
		roomRenderInfo.length = 1;
		roomRenderInfo.position = glm::vec2(.8f, -.75);
		roomRenderInfo.additionalSpacing = -CARD_SPACING;
		roomRenderInfo.state = RenderCardInfo::State::field;
		cards[0] = &info.rooms[state.paths[state.chosenPath].room];
		RenderCards(roomRenderInfo);

		cards[0] = &info.events[eventCard];
		roomRenderInfo.position.x *= -1;
		RenderCards(roomRenderInfo);

		for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
			cards[i] = &info.monsters[boardState.enemyIds[i]];

		uint32_t currentHealths[BOARD_CAPACITY_PER_SIDE]{};
		for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
			currentHealths[i] = boardState.enemyHealths[i];

		RenderMonsterCardInfo enemyRenderInfo{};
		enemyRenderInfo.levelUpdateInfo = &info;
		enemyRenderInfo.cards = cards;
		enemyRenderInfo.length = boardState.enemyMonsterCount;
		enemyRenderInfo.position.y = -CARD_HEIGHT_OFFSET;
		enemyRenderInfo.currentHealthArr = currentHealths;
		enemyRenderInfo.state = RenderCardInfo::State::field;
		const auto enemyChoice = RenderMonsterCards(info.frameArena, enemyRenderInfo).selectedMonster;

		for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
		{
			TextTask textTask{};
			textTask.center = true;
			textTask.text = TextInterpreter::IntToConstCharPtr(boardState.enemyTargets[i] + 1, info.frameArena);
			textTask.position = glm::vec2(-CARD_WIDTH_OFFSET * (boardState.enemyMonsterCount - 1) / 2 + CARD_WIDTH_OFFSET * i, -CARD_HEIGHT_OFFSET);
			textTask.scale = TEXT_MEDIUM_SCALE;
			info.textTasks.Push(textTask);
		}

		for (uint32_t i = 0; i < boardState.alliedMonsterCount; ++i)
			cards[i] = &info.monsters[boardState.allyIds[i]];

		bool selected[BOARD_CAPACITY_PER_SIDE]{};
		if(allySelected != -1 && !tapped[allySelected])
			selected[allySelected] = true;
		else
			for (uint32_t i = 0; i < BOARD_CAPACITY_PER_SIDE; ++i)
				selected[i] = !tapped[i];

		for (uint32_t i = 0; i < boardState.alliedMonsterCount; ++i)
			currentHealths[i] = boardState.allyHealths[i];

		ArtifactCard** artifacts[PARTY_ACTIVE_CAPACITY]{};
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};

		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const uint32_t partyId = gameState.partyIds[i];
			const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[partyId];
			if (count == 0)
				continue;
			const auto arr = artifacts[i] = static_cast<ArtifactCard**>(info.frameArena.Alloc(sizeof(void*) * count));
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		FlawCard* flaws[PARTY_ACTIVE_CAPACITY]{};
		for (uint32_t i = 0; i < boardState.partySize; ++i)
		{
			const uint32_t index = info.gameState.flaws[i];
			flaws[i] = index == -1 ? nullptr : &info.flaws[index];
		}

		RenderMonsterCardInfo alliedRenderInfo{};
		alliedRenderInfo.levelUpdateInfo = &info;
		alliedRenderInfo.cards = cards;
		alliedRenderInfo.length = boardState.alliedMonsterCount;
		alliedRenderInfo.position.y = CARD_HEIGHT_OFFSET * 2;
		alliedRenderInfo.selectedArr = selected;
		alliedRenderInfo.currentHealthArr = currentHealths;
		alliedRenderInfo.artifactArr = artifacts;
		alliedRenderInfo.artifactCounts = artifactCounts;
		alliedRenderInfo.flawArr = flaws;
		alliedRenderInfo.state = RenderCardInfo::State::field;
		const uint32_t allyChoice = RenderMonsterCards(info.frameArena, alliedRenderInfo).selectedMonster;
		
		if(info.inputState.lMouse == InputState::pressed && allyChoice != -1 && !tapped[allyChoice])
			allySelected = allyChoice;

		if(info.inputState.lMouse == InputState::released)
		{
			if(allySelected != -1 && enemyChoice != -1)
			{
				const uint32_t allyMonsterId = boardState.allyIds[allySelected];
				const auto& allyMonster = info.monsters[allyMonsterId];
				const uint32_t monsterId = boardState.enemyIds[enemyChoice];

				boardState.DealDamage(BOARD_CAPACITY_PER_SIDE + enemyChoice, allyMonster.attack);

				if (boardState.enemyMonsterCount == 0)
				{
					auto& gameState = info.gameState;
					for (uint32_t i = 0; i < boardState.partySize; ++i)
					{
						gameState.partyIds[i] = boardState.partyIds[i];
						gameState.healths[i] = boardState.allyHealths[i];
					}
					gameState.partySize = boardState.partySize;
					recruitableMonster = monsterId;
				}
					
				tapped[allySelected] = true;
			}
			allySelected = -1;
		}

		bool allTapped = true;
		for (uint32_t i = 0; i < boardState.alliedMonsterCount; ++i)
		{
			if(!tapped[i])
			{
				allTapped = false;
				break;
			}
		}
		if (allTapped || info.inputState.enter == InputState::pressed)
		{
			for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
			{
				const uint32_t enemyMonsterId = boardState.enemyIds[i];
				const auto& enemyMonster = info.monsters[enemyMonsterId];
				const uint32_t target = boardState.enemyTargets[i];
				if (target > boardState.alliedMonsterCount)
					continue;
				boardState.DealDamage(target, enemyMonster.attack);
				if (boardState.alliedMonsterCount == 0)
					loadLevelIndex = LevelIndex::mainMenu;
			}
			newTurn = true;
		}

		return true;
	}

	void MainLevel::RewardMagicCardState::Reset(State& state, const LevelInfo& info)
	{
		scroll = 0;
		discoverOption = -1;
	}

	bool MainLevel::RewardMagicCardState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_DECK_SIZE]{};

		for (uint32_t i = 0; i < MAGIC_DECK_SIZE; ++i)
			cards[i] = &info.magics[info.gameState.magics[i]];

		scroll += info.inputState.scroll * .1f;
		scroll = jv::Clamp<float>(scroll, -1, 1);

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.length = MAGIC_DECK_SIZE;
		renderInfo.highlight = discoverOption;
		renderInfo.cards = cards;
		renderInfo.position.x = scroll;
		renderInfo.position.y = CARD_HEIGHT;
		renderInfo.additionalSpacing = -CARD_SPACING;
		renderInfo.lineLength = MAGIC_DECK_SIZE / 2;
		const uint32_t choice = RenderMagicCards(info.frameArena, renderInfo);

		const auto& path = state.paths[state.chosenPath];

		cards[0] = &info.magics[path.magic];
		renderInfo.length = 1;
		renderInfo.highlight = -1;
		renderInfo.position.y *= -1;
		renderInfo.position.x = 0;
		RenderMagicCards(info.frameArena, renderInfo);

		if (info.inputState.lMouse == InputState::pressed)
			discoverOption = choice == discoverOption ? -1 : choice;

		TextTask textTask{};
		textTask.center = true;
		textTask.lineLength = 20;
		textTask.scale = TEXT_BIG_SCALE;
		textTask.position = TEXT_CENTER_BOT_POSITION;
		textTask.text = "press enter to continue.";
		info.textTasks.Push(textTask);

		textTask.text = "select card to replace, if any.";
		textTask.position = TEXT_CENTER_TOP_POSITION;
		info.textTasks.Push(textTask);

		if (info.inputState.enter == InputState::pressed)
		{
			if (discoverOption != -1)
				info.gameState.magics[discoverOption] = path.magic;

			if (state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW)
				stateIndex = static_cast<uint32_t>(StateNames::rewardFlaw);
			else if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
				stateIndex = static_cast<uint32_t>(StateNames::rewardArtifact);
			else
				stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		}
				
		return true;
	}

	void MainLevel::RewardFlawCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
	}

	bool MainLevel::RewardFlawCardState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_DECK_SIZE]{};

		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		bool selected[PARTY_ACTIVE_CAPACITY];
		bool flawSlotAvailable = false;

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const auto& flaw = info.gameState.flaws[i];
			selected[i] = flaw == -1;
			flawSlotAvailable = flawSlotAvailable ? true : flaw == -1;
		}

		if (flawSlotAvailable)
		{
			TextTask textTask{};
			textTask.center = true;
			textTask.lineLength = 20;
			textTask.scale = TEXT_BIG_SCALE;
			textTask.position = TEXT_CENTER_TOP_POSITION;
			textTask.text = "select one ally to carry this flaw.";
			info.textTasks.Push(textTask);

			for (uint32_t i = 0; i < gameState.partySize; ++i)
				cards[i] = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];

			RenderMonsterCardInfo renderInfo{};
			renderInfo.levelUpdateInfo = &info;
			renderInfo.length = gameState.partySize;
			renderInfo.cards = cards;
			renderInfo.selectedArr = selected;
			renderInfo.highlight = discoverOption;
			renderInfo.position.y = CARD_HEIGHT;
			renderInfo.additionalSpacing = -CARD_SPACING;
			const uint32_t choice = RenderMonsterCards(info.frameArena, renderInfo).selectedMonster;

			const auto& path = state.paths[state.chosenPath];

			cards[0] = &info.flaws[path.flaw];
			renderInfo.position.y *= -1;
			renderInfo.length = 1;
			renderInfo.selectedArr = nullptr;
			renderInfo.highlight = -1;
			RenderCards(renderInfo);

			if (info.inputState.lMouse == InputState::pressed)
				if (choice == -1 || selected[choice])
					discoverOption = choice == discoverOption ? -1 : choice;
			
			if (discoverOption != -1)
			{
				textTask.position = TEXT_CENTER_BOT_POSITION;
				textTask.text = "press enter to continue.";
				info.textTasks.Push(textTask);

				if (info.inputState.enter != InputState::pressed)
					return true;
				info.gameState.flaws[gameState.partyIds[discoverOption]] = path.flaw;
				stateIndex = static_cast<uint32_t>(StateNames::exitFound);
			}
			return true;
		}

		stateIndex = static_cast<uint32_t>(StateNames::exitFound);
		return true;
	}

	void MainLevel::RewardArtifactState::Reset(State& state, const LevelInfo& info)
	{
		if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
			for (auto& slotCount : info.playerState.artifactSlotCounts)
				slotCount = jv::Max(slotCount, state.depth / ROOM_COUNT_BEFORE_BOSS);
	}

	bool MainLevel::RewardArtifactState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[PARTY_ACTIVE_CAPACITY]{};
		
		auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		TextTask textTask{};
		textTask.center = true;
		textTask.lineLength = 20;
		textTask.scale = .06f;
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.text = "you can switch your artifacts around.";
		info.textTasks.Push(textTask);

		textTask.position = TEXT_CENTER_BOT_POSITION;
		textTask.text = " press enter to continue.";
		info.textTasks.Push(textTask);

		for (uint32_t i = 0; i < gameState.partySize; ++i)
			cards[i] = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];

		const auto tempScope = info.tempArena.CreateScope();

		ArtifactCard** artifacts[PARTY_ACTIVE_CAPACITY]{};
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};

		for (uint32_t i = 0; i < playerState.partySize; ++i)
		{
			const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[i];
			if (count == 0)
				continue;
			const auto arr = artifacts[i] = static_cast<ArtifactCard**>(info.tempArena.Alloc(sizeof(void*) * count));
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		RenderMonsterCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.length = gameState.partySize;
		renderInfo.cards = cards;
		renderInfo.position.y = CARD_HEIGHT_OFFSET;
		renderInfo.artifactArr = artifacts;
		renderInfo.artifactCounts = artifactCounts;
		const auto choice = RenderMonsterCards(info.frameArena, renderInfo);

		info.tempArena.DestroyScope(tempScope);

		auto& path = state.paths[state.chosenPath];
		
		cards[0] = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
		renderInfo.position.y *= -1;
		renderInfo.length = 1;
		RenderCards(renderInfo);

		if (info.inputState.lMouse == InputState::pressed && choice.selectedArtifact != -1 && 
			choice.selectedArtifact < playerState.artifactSlotCounts[choice.selectedMonster])
		{
			const uint32_t swappable = path.artifact;
			path.artifact = playerState.artifacts[choice.selectedMonster * MONSTER_ARTIFACT_CAPACITY + choice.selectedArtifact];
			playerState.artifacts[choice.selectedMonster * MONSTER_ARTIFACT_CAPACITY + choice.selectedArtifact] = swappable;
		}

		if (info.inputState.enter == InputState::pressed)
			stateIndex = static_cast<uint32_t>(StateNames::exitFound);
		return true;
	}

	bool MainLevel::ExitFoundState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		TextTask textTask{};
		textTask.center = true;
		textTask.lineLength = 20;
		textTask.scale = TEXT_BIG_SCALE;
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.text = "an exit leading outside has been found.";
		info.textTasks.Push(textTask);

		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = -.18;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::fallback)].subTexture;
		info.renderTasks.Push(buttonRenderTask);

		if (info.inputState.lMouse == InputState::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
			{
				stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
				return true;
			}

		TextTask buttonTextTask{};
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "continue forward";
		buttonTextTask.scale = TEXT_BIG_SCALE;
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
		return true;
	}

	void MainLevel::Create(const LevelCreateInfo& info)
	{
		if (info.playerState.ironManMode)
			ClearSaveData();

		const auto states = jv::CreateArray<LevelState<State>*>(info.arena, 7);
		states[0] = info.arena.New<BossRevealState>();
		states[1] = info.arena.New<PathSelectState>();
		states[2] = info.arena.New<CombatState>();
		states[3] = info.arena.New<RewardMagicCardState>();
		states[4] = info.arena.New<RewardFlawCardState>();
		states[5] = info.arena.New<RewardArtifactState>();
		states[6] = info.arena.New<ExitFoundState>();
		stateMachine = LevelStateMachine<State>::Create(info, states, State::Create(info));
	}

	bool MainLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		return stateMachine.Update(info, loadLevelIndex);
	}
}
