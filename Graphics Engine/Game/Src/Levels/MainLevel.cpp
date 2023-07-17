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
					if (monsters[j] == boardState.monsterIds[j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < boardState.enemyMonsterCount; ++j)
					if (monsters[j] == boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + j])
					{
						monsters.RemoveAt(i);
						removed = true;
						break;
					}
				if (removed)
					continue;

				for (uint32_t j = 0; j < info.playerState.partySize; ++j)
					if(monsters[j] == info.playerState.monsterIds[j])
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

	MainLevel::State MainLevel::State::Create(const LevelCreateInfo& info)
	{
		State state{};
		state.decks = Decks::Create(info);
		state.paths = jv::CreateArray<Path>(info.arena, DISCOVER_LENGTH);
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
		renderInfo.center = glm::vec2(0, -CARD_HEIGHT);
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
			renderInfo.center.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}
		// Render artifacts.
		else if (state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1)
		{
			for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
				cards[i] = &info.artifacts[state.paths[i].artifact];
			renderInfo.center.x += CARD_WIDTH * 2;
			const auto choice = RenderCards(renderInfo);
			selected = choice != -1 ? choice : selected;
		}

		// Render rooms.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.rooms[state.paths[i].room];

		renderInfo.center = glm::vec2(0, CARD_HEIGHT);
		renderInfo.additionalSpacing = CARD_WIDTH_OFFSET;
		auto choice = RenderCards(renderInfo);
		selected = choice != -1 ? choice : selected;

		// Render magics.
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			cards[i] = &info.magics[state.paths[i].magic];

		renderInfo.center.x += CARD_WIDTH * 2;
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
		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		boardState = {};
		turnState = TurnState::untap;

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const uint32_t partyMemberId = gameState.partyMembers[i];
			boardState.monsterIds[i] = playerState.monsterIds[partyMemberId];
			boardState.healths[i] = gameState.healths[i];
			boardState.partyIds[i] = partyMemberId;
		}
		boardState.alliedMonsterCount = gameState.partySize;
		boardState.partyCount = gameState.partySize;

		const uint32_t enemyCount = MONSTER_CAPACITIES[jv::Min((state.depth - 1) / ROOM_COUNT_BEFORE_BOSS, TOTAL_BOSS_COUNT - 1)];
		for (uint32_t i = 0; i < enemyCount; ++i)
			boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + i] = state.GetMonster(info, boardState);
		boardState.enemyMonsterCount = enemyCount;
	}

	bool MainLevel::CombatState::Update(State& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		if(turnState == TurnState::untap)
		{
			for (auto& b : tapped)
				b = false;
			turnState = TurnState::allied;
			selectedAlly = -1;
		}

		Card* cards[BOARD_CAPACITY_PER_SIDE]{};

		for (uint32_t i = 0; i < boardState.alliedMonsterCount; ++i)
			cards[i] = &info.monsters[boardState.monsterIds[i]];

		RenderCardInfo enemyRenderInfo{};
		enemyRenderInfo.levelUpdateInfo = &info;
		enemyRenderInfo.cards = cards;
		enemyRenderInfo.length = boardState.enemyMonsterCount;
		enemyRenderInfo.center.y = -CARD_HEIGHT_OFFSET;
		const auto enemyChoice = RenderMonsterCards(info.frameArena, enemyRenderInfo);

		bool selected[BOARD_CAPACITY_PER_SIDE]{};
		for (uint32_t i = 0; i < BOARD_CAPACITY_PER_SIDE; ++i)
			selected[i] = !tapped[i];

		// If ally has been selected.
		if (selectedAlly != -1)
		{
			for (auto& b : selected)
				b = false;
			selected[selectedAlly] = true;
		}

		RenderCardInfo alliedRenderInfo{};
		alliedRenderInfo.levelUpdateInfo = &info;
		alliedRenderInfo.cards = cards;
		alliedRenderInfo.length = boardState.alliedMonsterCount;
		alliedRenderInfo.center.y = CARD_HEIGHT_OFFSET;
		alliedRenderInfo.selectedArr = selected;
		const uint32_t allyChoice = RenderMonsterCards(info.frameArena, alliedRenderInfo);

		if(info.inputState.lMouse == InputState::pressed)
		{
			if(selectedAlly != -1 && enemyChoice != -1)
			{
				const auto& allyMonster = info.monsters[boardState.monsterIds[selectedAlly]];
				auto& health = boardState.healths[BOARD_CAPACITY_PER_SIDE + enemyChoice];

				if(health <= allyMonster.attack)
				{
					for (uint32_t i = enemyChoice; i < boardState.enemyMonsterCount; ++i)
					{
						boardState.healths[BOARD_CAPACITY_PER_SIDE + i] = boardState.healths[BOARD_CAPACITY_PER_SIDE + i + 1];
						boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + i] = boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + i + 1];
					}

					boardState.enemyMonsterCount--;
				}
				else
					health -= allyMonster.attack;
				tapped[selectedAlly] = true;
			}

			if(allyChoice != -1 && !tapped[allyChoice])
				selectedAlly = allyChoice;
		}

		if (selectedAlly  != -1)
		{
			const uint32_t artifactSlotCount = info.playerState.artifactSlotCounts[selectedAlly];
			for (uint32_t i = 0; i < artifactSlotCount; ++i)
			{
				const uint32_t partyMemberId = info.gameState.partyMembers[selectedAlly];
				const uint32_t index = info.playerState.artifacts[partyMemberId * MONSTER_ARTIFACT_CAPACITY + i];
				cards[i] = &info.artifacts[index];
			}

			RenderCardInfo artifactRenderInfo{};
			artifactRenderInfo.levelUpdateInfo = &info;
			artifactRenderInfo.cards = cards;
			artifactRenderInfo.length = artifactSlotCount;
			artifactRenderInfo.center.y += CARD_HEIGHT_OFFSET + CARD_HEIGHT * 2;
			RenderCards(artifactRenderInfo);
		}

		for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
			cards[i] = &info.monsters[boardState.monsterIds[BOARD_CAPACITY_PER_SIDE + i]];

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
		Card* cards[MAGIC_CAPACITY]{};

		for (uint32_t i = 0; i < MAGIC_CAPACITY; ++i)
			cards[i] = &info.magics[info.gameState.magics[i]];

		scroll += info.inputState.scroll * .1f;
		scroll = jv::Clamp<float>(scroll, -1, 1);

		RenderCardInfo renderInfo{};
		renderInfo.levelUpdateInfo = &info;
		renderInfo.length = MAGIC_CAPACITY;
		renderInfo.highlight = discoverOption;
		renderInfo.cards = cards;
		renderInfo.center.x = scroll;
		renderInfo.center.y = CARD_HEIGHT;
		renderInfo.additionalSpacing = -CARD_SPACING;
		renderInfo.lineLength = MAGIC_CAPACITY / 2;
		const uint32_t choice = RenderMagicCards(info.frameArena, renderInfo);

		auto& path = state.paths[state.chosenPath];

		cards[0] = &info.magics[path.magic];
		renderInfo.length = 1;
		renderInfo.highlight = -1;
		renderInfo.center.y *= -1;
		renderInfo.center.x = 0;
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
		Card* cards[MAGIC_CAPACITY]{};

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
				cards[i] = &info.monsters[playerState.monsterIds[gameState.partyMembers[i]]];

			RenderCardInfo renderInfo{};
			renderInfo.levelUpdateInfo = &info;
			renderInfo.length = gameState.partySize;
			renderInfo.cards = cards;
			renderInfo.selectedArr = selected;
			renderInfo.highlight = discoverOption;
			renderInfo.center.y = CARD_HEIGHT;
			renderInfo.additionalSpacing = -CARD_SPACING;
			const uint32_t choice = RenderMonsterCards(info.frameArena, renderInfo);

			const auto& path = state.paths[state.chosenPath];

			cards[0] = &info.flaws[path.flaw];
			renderInfo.center.y *= -1;
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
				info.gameState.flaws[discoverOption] = path.flaw;
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
		Card* cards[MAGIC_CAPACITY]{};
		
		auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

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
		RenderMonsterCards(info.frameArena, renderInfo);

		auto& path = state.paths[state.chosenPath];
		
		cards[0] = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
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
			if (info.inputState.lMouse == InputState::pressed && choice != -1 && choice < unlockedCount)
			{
				const uint32_t swappable = path.artifact;
				path.artifact = playerState.artifacts[artifactStartIndex + choice];
				playerState.artifacts[artifactStartIndex + choice] = swappable;
			}

			renderInfo.center.x += CARD_WIDTH_OFFSET * 2;
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
		buttonRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
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
