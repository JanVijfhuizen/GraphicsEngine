﻿#include "pch_game.h"
#include "Levels/MainLevel.h"
#include <Levels/LevelUtils.h>
#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"
#include "States/InputState.h"
#include "States/GameState.h"
#include "States/BoardState.h"
#include "States/PlayerState.h"
#include "Utils/Shuffle.h"
#include "JLib/VectorUtils.h"

namespace game
{
	void MainLevel::BossRevealState::Reset(State& state, const LevelInfo& info)
	{
		for (auto& path : state.paths)
		{
			path = {};
			if (state.depth != SUB_BOSS_COUNT * ROOM_COUNT_BEFORE_BOSS)
				path.boss = state.GetBoss(info);
			else
				path.boss = FINAL_BOSS_ID;
		}
		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
	}

	bool MainLevel::BossRevealState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[DISCOVER_LENGTH]{};
		CombatStats combatStats[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto monster = &info.monsters[state.paths[i].boss];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
		}

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		cardSelectionDrawInfo.hoverDurations = hoverDurations;
		cardSelectionDrawInfo.combatStats = combatStats;
		level->DrawCardSelection(info, cardSelectionDrawInfo);

		const char* text = "the stage bosses have been revealed.";
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);
		if(f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, f);
		
		if (info.inputState.enter.PressEvent())
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
		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
	}

	bool MainLevel::PathSelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const bool flawPresent = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_FLAW - 1;
		const bool bossPresent = state.depth % ROOM_COUNT_BEFORE_BOSS == ROOM_COUNT_BEFORE_BOSS - 1;

		TextTask depthTextTask{};
		depthTextTask.text = TextInterpreter::IntToConstCharPtr(state.depth + 1, info.frameArena);
		depthTextTask.text = TextInterpreter::Concat("depth-", depthTextTask.text, info.frameArena);
		depthTextTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + 72);
		depthTextTask.center = true;
		depthTextTask.lifetime = level->GetTime();
		info.textTasks.Push(depthTextTask);

		// Render bosses.
		Card* cards[DISCOVER_LENGTH];
		CombatStats combatStats[DISCOVER_LENGTH]{};

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto& path = state.paths[i];
			if(!bossPresent)
			{
				const auto monster = &info.monsters[state.paths[i].boss];
				cards[i] = monster;
				combatStats[i] = GetCombatStat(*monster);
			}
			else
				cards[i] = &info.artifacts[path.artifact];
		}

		Card** stacks[DISCOVER_LENGTH]{};
		for (auto& stack : stacks)
			stack = jv::CreateArray<Card*>(info.frameArena, 3).ptr;

		uint32_t stacksCounts[DISCOVER_LENGTH]{};
		for (auto& c : stacksCounts)
			c = 2 + flawPresent;

		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto& stack = stacks[i];
			const auto& path = state.paths[i];

			stack[0] = &info.rooms[path.room];
			stack[1] = &info.magics[path.magic];
			if (flawPresent)
				stack[2] = &info.flaws[path.flaw];
		}

		const char* texts[DISCOVER_LENGTH]{};
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			uint32_t counters = state.paths[i].counters;
			counters += i == discoverOption;
			texts[i] = TextInterpreter::IntToConstCharPtr(counters, info.frameArena);
		}
		
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.stacks = stacks;
		cardSelectionDrawInfo.stackCounts = stacksCounts;
		cardSelectionDrawInfo.texts = bossPresent ? nullptr : texts;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.hoverDurations = hoverDurations;
		if (!bossPresent)
			cardSelectionDrawInfo.combatStats = combatStats;
		uint32_t selected = level->DrawCardSelection(info, cardSelectionDrawInfo);

		if (bossPresent)
		{
			const auto monster = &info.monsters[state.paths[state.GetPrimaryPath()].boss];
			auto combatStat = GetCombatStat(*monster);

			const auto shape = GetCardShape(info, cardSelectionDrawInfo);

			CardDrawInfo cardDrawInfo{};
			cardDrawInfo.card = monster;
			cardDrawInfo.combatStats = &combatStat;
			cardDrawInfo.center = true;
			cardDrawInfo.origin = SIMULATED_RESOLUTION / 2;
			cardDrawInfo.origin.x += (shape.x + cardSelectionDrawInfo.offsetMod) * 4 / 2;
			level->DrawCard(info, cardDrawInfo);
		}

		if (info.inputState.lMouse.PressEvent())
		{
			discoverOption = selected == discoverOption ? -1 : selected;
			if (selected != -1)
				timeSinceDiscovered = level->GetTime();
		}

		const char* text = "select which road to take.";
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);
		
		if (discoverOption != -1)
		{
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, level->GetTime() - timeSinceDiscovered);
			
			if (info.inputState.enter.PressEvent())
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
		const bool bossPresent = state.depth != 0 && state.depth % ROOM_COUNT_BEFORE_BOSS == 0;
		const auto& gameState = info.gameState;

		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
		
		selectionState = SelectionState::none;
		time = 0;
		selectedId = -1;
		uniqueId = 0;
		mana = 0;
		maxMana = 0;
		timeSinceLastActionState = 0;
		activeState = nullptr;
		eventCard = -1;
		previousEventCard = -1;
		state.boardState = {};
		state.hand.Clear();
		state.stack.Clear();
		for (bool& b : tapped)
			b = false;

		ActionState startOfTurnActionState{};
		startOfTurnActionState.trigger = ActionState::Trigger::onStartOfTurn;
		state.stack.Add() = startOfTurnActionState;

		for (uint32_t i = 0; i < HAND_INITIAL_SIZE; ++i)
		{
			ActionState drawState{};
			drawState.trigger = ActionState::Trigger::draw;
			state.stack.Add() = drawState;
		}

		if (!bossPresent)
		{
			const auto enemyCount = jv::Min<uint32_t>(4, state.depth + 1);
			for (uint32_t i = 0; i < enemyCount; ++i)
			{
				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = state.GetMonster(info); 
				state.stack.Add() = summonState;
			}
		}
		else
		{
			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = state.paths[state.GetPrimaryPath()].boss;
			state.stack.Add() = summonState;
		}
		
		for (int32_t i = static_cast<int32_t>(gameState.partyCount) - 1; i >= 0; --i)
		{
			const auto partyId = gameState.partyIds[i];
			const auto monsterId = gameState.monsterIds[i];

			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 1;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = monsterId;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::partyId)] = partyId;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::health)] = gameState.healths[i];
			state.stack.Add() = summonState;
		}
	}

	bool MainLevel::CombatState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		time += info.deltaTime;

		auto& gameState = info.gameState;
		auto& boardState = state.boardState;

		if (state.stack.count > 0)
		{
			bool valid = true;
			if (!activeState)
			{
				timeSinceLastActionState = level->GetTime();
				actionStateDuration = ACTION_STATE_DEFAULT_DURATION;

				auto& actionState = state.stack.Peek();
				activeState = &actionState;

				// Temp.
				switch (actionState.trigger)
				{
				case ActionState::Trigger::onAttack:
					actionStateDuration = ATTACK_ACTION_STATE_DURATION;
					break;
				case ActionState::Trigger::onStartOfTurn:
					actionStateDuration = START_OF_TURN_ACTION_STATE_DURATION;
					break;
				default:
					break;
				}

				valid = PreHandleActionState(state, info, actionState);
				if (!valid)
					state.stack.Pop();
			}

			if (valid)
			{
				if (timeSinceLastActionState + actionStateDuration < level->GetTime())
				{
					auto actionState = state.stack.Pop();
					valid = PostHandleActionState(state, info, actionState);
					activeState = nullptr;
				}
			}
			else
				activeState = nullptr;
		}

		if(activeState && activeState->trigger == ActionState::Trigger::onStartOfTurn)
		{
			PixelPerfectRenderTask lineRenderTask{};
			lineRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;
			lineRenderTask.scale.x = SIMULATED_RESOLUTION.x;
			lineRenderTask.scale.y = 1;
			lineRenderTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, CENTER_HEIGHT);
			lineRenderTask.priority = true;

			const float l = GetActionStateLerp(*level, START_OF_TURN_ACTION_STATE_DURATION);
			const auto curve = je::CreateCurvePauseInMiddle();
			const float eval = (curve.Evaluate(l) - .5f) * 2;
			const float off = eval * SIMULATED_RESOLUTION.x;

			constexpr float textDuration = START_OF_TURN_ACTION_STATE_DURATION / 2;
			constexpr float l2Start = .5f - textDuration / 2;

			const float l2Dis = l - l2Start;
			if(l2Dis >= 0 && l2Dis <= textDuration)
			{
				const float f = l2Dis / textDuration;
				const float eval2 = (curve.Evaluate(f) - .5f) * 2;
				const float off2 = eval2 * SIMULATED_RESOLUTION.x;

				TextTask textTask{};
				textTask.text = "new";
				textTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, CENTER_HEIGHT) + glm::ivec2(off2, 1);
				textTask.scale = 2;
				textTask.center = true;
				textTask.priority = true;

				info.textTasks.Push(textTask);
				textTask.text = "turn";
				textTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, CENTER_HEIGHT) - glm::ivec2(off2, 18);
				info.textTasks.Push(textTask);
			}

			lineRenderTask.position.x = off;
			info.pixelPerfectRenderTasks.Push(lineRenderTask);
		}

		if(state.stack.count == 0)
		{
			// Check for game over.
			if (boardState.allyCount == 0)
			{
				loadLevelIndex = LevelIndex::mainMenu;
				return true;
			}

			// Check for room victory.
			if (boardState.enemyCount == 0)
			{
				if (boardState.allyCount < PARTY_ACTIVE_CAPACITY)
				{
					const auto monster = &info.monsters[lastEnemyDefeatedId];
					if (!monster->unique)
					{
						// Recruitment.
						level->DrawTopCenterHeader(info, HeaderSpacing::normal, "someone wants to join your party.");
						bool recruitScreenActive = true;
						auto combatStats = GetCombatStat(*monster);

						CardDrawInfo cardDrawInfo{};
						cardDrawInfo.card = monster;
						cardDrawInfo.center = true;
						cardDrawInfo.combatStats = &combatStats;
						cardDrawInfo.origin = SIMULATED_RESOLUTION / 2;
						level->DrawCard(info, cardDrawInfo);

						ButtonDrawInfo buttonAcceptDrawInfo{};
						buttonAcceptDrawInfo.origin = SIMULATED_RESOLUTION / 2 + glm::ivec2(0, 28);
						buttonAcceptDrawInfo.text = "accept";
						buttonAcceptDrawInfo.center = true;
						if (level->DrawButton(info, buttonAcceptDrawInfo))
						{
							// Add to party.
							boardState.ids[boardState.allyCount] = lastEnemyDefeatedId;
							boardState.combatStats[boardState.allyCount] = GetCombatStat(info.monsters[lastEnemyDefeatedId]);
							boardState.partyIds[boardState.allyCount++] = -1;
							recruitScreenActive = false;
						}
						ButtonDrawInfo buttonDeclineDrawInfo{};
						buttonDeclineDrawInfo.origin = SIMULATED_RESOLUTION / 2 - glm::ivec2(0, 36);
						buttonDeclineDrawInfo.text = "decline";
						buttonDeclineDrawInfo.center = true;
						if (level->DrawButton(info, buttonDeclineDrawInfo))
							recruitScreenActive = false;

						if (recruitScreenActive)
							return true;
					}
				}

				gameState.partyCount = jv::Min(boardState.allyCount, PARTY_ACTIVE_CAPACITY);
				for (uint32_t i = 0; i < gameState.partyCount; ++i)
				{
					gameState.partyIds[i] = boardState.partyIds[i];
					gameState.monsterIds[i] = boardState.ids[i];
					gameState.healths[i] = boardState.combatStats[i].health;
				}

				stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);
				return true;
			}
		}

		{
			const bool isStartOfTurn = activeState && activeState->trigger == ActionState::Trigger::onStartOfTurn;
			auto cards = jv::CreateVector<Card*>(info.frameArena, 3);
			cards.Add() = &info.rooms[state.paths[state.chosenPath].room];
			if (eventCard != -1)
				cards.Add() = &info.events[eventCard];
			if (isStartOfTurn && previousEventCard != -1)
				cards.Add() = &info.events[previousEventCard];

			CardSelectionDrawInfo eventSelectionDrawInfo{};
			eventSelectionDrawInfo.lifeTime = level->GetTime();
			eventSelectionDrawInfo.cards = cards.ptr;
			eventSelectionDrawInfo.length = cards.count;
			eventSelectionDrawInfo.height = ALLY_HEIGHT;
			eventSelectionDrawInfo.hoverDurations = hoverDurations;
			eventSelectionDrawInfo.centerOffset = -SIMULATED_RESOLUTION.x / 2 + 32;
			eventSelectionDrawInfo.offsetMod = -4;
			if (isStartOfTurn)
			{
				eventSelectionDrawInfo.spawnRight = false;
				DrawDrawAnimation(info, *level, eventSelectionDrawInfo);
				if (cards.count > 2)
				{
					DrawFadeAnimation(*level, eventSelectionDrawInfo, 1);
				}
			}

			level->DrawCardSelection(info, eventSelectionDrawInfo);
		}

		constexpr uint32_t LINE_POSITIONS[]{ ALLY_HEIGHT };

		PixelPerfectRenderTask lineRenderTask{};
		lineRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;
		lineRenderTask.scale.x = SIMULATED_RESOLUTION.x;
		lineRenderTask.scale.y = 1;

		for (const auto& i : LINE_POSITIONS)
		{
			lineRenderTask.position.y = i;
			info.pixelPerfectRenderTasks.Push(lineRenderTask);
		}

		// Check for new turn.
		if(state.stack.count == 0)
		{
			// Manually end turn.
			if (info.inputState.enter.PressEvent())
			{
				ActionState startOfTurnActionState{};
				startOfTurnActionState.trigger = ActionState::Trigger::onStartOfTurn;
				state.stack.Add() = startOfTurnActionState;

				for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				{
					const uint32_t target = targets[i];
					if (boardState.allyCount >= target)
					{
						ActionState attackActionState{};
						attackActionState.trigger = ActionState::Trigger::onAttack;
						attackActionState.src = BOARD_CAPACITY_PER_SIDE + i;
						attackActionState.dst = target;
						attackActionState.srcUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
						attackActionState.dstUniqueId = boardState.uniqueIds[target];
						state.stack.Add() = attackActionState;
					}
				}
			}
		}

		const auto& lMouse = info.inputState.lMouse;
		const bool lMousePressed = lMouse.PressEvent();
		const bool lMouseReleased = lMouse.ReleaseEvent();

		constexpr uint32_t l = HAND_MAX_SIZE > BOARD_CAPACITY_PER_SIDE ? HAND_MAX_SIZE : BOARD_CAPACITY_PER_SIDE;

		Card* cards[l]{};
		for (uint32_t i = 0; i < boardState.enemyCount; ++i)
			cards[i] = &info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]];
		
		bool* selectedArr = nullptr;

		if(selectionState == SelectionState::enemy)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		// Draw enemies.
		CardSelectionDrawInfo enemySelectionDrawInfo{};
		enemySelectionDrawInfo.lifeTime = level->GetTime();
		enemySelectionDrawInfo.cards = cards;
		enemySelectionDrawInfo.length = boardState.enemyCount;
		enemySelectionDrawInfo.height = ENEMY_HEIGHT;
		enemySelectionDrawInfo.costs = targets;
		enemySelectionDrawInfo.selectedArr = selectedArr;
		enemySelectionDrawInfo.combatStats = &boardState.combatStats[BOARD_CAPACITY_PER_SIDE];
		enemySelectionDrawInfo.hoverDurations = &hoverDurations[2];
		DrawAttackAnimation(state, info, *level, enemySelectionDrawInfo, false);
		DrawDamageAnimation(info, *level, enemySelectionDrawInfo, false);
		DrawSummonAnimation(info, *level, enemySelectionDrawInfo, false);
		DrawDeathAnimation(info, *level, enemySelectionDrawInfo, false);
		const auto enemyResult = level->DrawCardSelection(info, enemySelectionDrawInfo);
		selectedArr = nullptr;

		if (state.stack.count == 0 && lMousePressed && enemyResult != -1)
		{
			selectionState = SelectionState::enemy;
			selectedId = enemyResult;
		}

		if (state.stack.count == 0 && selectionState == SelectionState::hand)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		bool greyedOutArr[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
			greyedOutArr[i] = info.magics[state.hand[i]].cost > mana;

		// Draw hand.
		uint32_t costs[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
		{
			const auto magic = &info.magics[state.hand[i]];
			cards[i] = magic;
			costs[i] = magic->cost;
		}

		CardSelectionDrawInfo handSelectionDrawInfo{};
		handSelectionDrawInfo.lifeTime = level->GetTime();
		handSelectionDrawInfo.cards = cards;
		handSelectionDrawInfo.length = state.hand.count;
		handSelectionDrawInfo.height = 32;
		handSelectionDrawInfo.texts = nullptr;
		handSelectionDrawInfo.offsetMod = -4;
		handSelectionDrawInfo.selectedArr = selectedArr;
		handSelectionDrawInfo.greyedOutArr = greyedOutArr;
		handSelectionDrawInfo.costs = costs;
		handSelectionDrawInfo.combatStats = nullptr;
		handSelectionDrawInfo.hoverDurations = &hoverDurations[2 + BOARD_CAPACITY_PER_SIDE];
		DrawCardPlayAnimation(*level, handSelectionDrawInfo);
		if(activeState && activeState->trigger == ActionState::Trigger::draw)
			DrawDrawAnimation(info, *level, handSelectionDrawInfo);

		const auto handResult = level->DrawCardSelection(info, handSelectionDrawInfo);
		selectedArr = nullptr;

		if(state.stack.count == 0 && lMousePressed && handResult != -1)
		{
			selectionState = SelectionState::hand;
			selectedId = handResult;
		}

		if (state.stack.count == 0 && selectionState == SelectionState::ally)
		{
			selectedArr = jv::CreateArray<bool>(info.frameArena, l).ptr;
			selectedArr[selectedId] = true;
		}

		// Draw allies.
		const auto& playerState = info.playerState;

		Card* playerCards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < boardState.allyCount; ++i)
			playerCards[i] = &info.monsters[boardState.ids[i]];

		Card** stacks[PARTY_CAPACITY]{};
		uint32_t stacksCount[PARTY_CAPACITY]{};

		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const auto partyId = boardState.partyIds[i];

			if(boardState.partyIds[i] == -1)
			{
				stacksCount[i] = 0;
				continue;
			}

			const uint32_t flaw = gameState.flaws[i];
			const uint32_t artifactCount = playerState.artifactSlotCounts[partyId];
			const uint32_t count = stacksCount[i] = (flaw != -1) + artifactCount;

			if (count == 0)
				continue;
			const auto arr = jv::CreateArray<Card*>(info.frameArena, count);
			stacks[i] = arr.ptr;
			for (uint32_t j = 0; j < artifactCount; ++j)
			{
				const uint32_t index = playerState.artifacts[partyId * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
			if(flaw != -1)
				stacks[i][artifactCount] = &info.flaws[flaw];
		}
		
		CardSelectionDrawInfo playerCardSelectionDrawInfo{};
		playerCardSelectionDrawInfo.cards = playerCards;
		playerCardSelectionDrawInfo.length = boardState.allyCount;
		playerCardSelectionDrawInfo.height = ALLY_HEIGHT;
		playerCardSelectionDrawInfo.stacks = stacks;
		playerCardSelectionDrawInfo.stackCounts = stacksCount;
		playerCardSelectionDrawInfo.lifeTime = level->GetTime();
		playerCardSelectionDrawInfo.greyedOutArr = tapped;
		playerCardSelectionDrawInfo.combatStats = boardState.combatStats;
		playerCardSelectionDrawInfo.selectedArr = selectedArr;
		playerCardSelectionDrawInfo.hoverDurations = &hoverDurations[2 + BOARD_CAPACITY_PER_SIDE + HAND_MAX_SIZE];
		DrawAttackAnimation(state, info, *level, playerCardSelectionDrawInfo, true);
		DrawDamageAnimation(info, *level, playerCardSelectionDrawInfo, true);
		DrawSummonAnimation(info, *level, playerCardSelectionDrawInfo, true);
		DrawDeathAnimation(info, *level, playerCardSelectionDrawInfo, true);

		const auto allyResult = level->DrawCardSelection(info, playerCardSelectionDrawInfo);
		
		if (state.stack.count == 0 && lMousePressed && allyResult != -1)
		{
			selectionState = SelectionState::ally;
			selectedId = allyResult;
		}

		// Draw mana.
		{
			TextTask manaTextTask{};
			manaTextTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, 4);
			manaTextTask.text = TextInterpreter::IntToConstCharPtr(mana, info.frameArena);
			manaTextTask.lifetime = level->GetTime();
			manaTextTask.center = true;
			info.textTasks.Push(manaTextTask);
		}

		if (state.stack.count == 0 && lMouseReleased)
		{
			// If player tried to attack an enemy.
			if (selectionState == SelectionState::ally)
			{
				auto& isTapped = tapped[selectedId];
				if (!isTapped && enemyResult != -1)
				{
					ActionState attackActionState{};
					attackActionState.trigger = ActionState::Trigger::onAttack;
					attackActionState.src = selectedId;
					attackActionState.dst = BOARD_CAPACITY_PER_SIDE + enemyResult;
					attackActionState.srcUniqueId = boardState.uniqueIds[selectedId];
					attackActionState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + enemyResult];
					state.stack.Add() = attackActionState;
				}
			}
			else if(selectionState == SelectionState::hand)
			{
				// Do something with the target.
				const auto& card = info.magics[selectedId];
				const bool validAll = card.type == MagicCard::Type::all && info.inputState.mousePos.y > SIMULATED_RESOLUTION.y / 4;
				const bool validTarget = card.type == MagicCard::Type::target && (enemyResult != -1 || allyResult != -1);
				const bool validPlay = validAll || validTarget;

				if(validPlay && card.cost <= mana)
				{
					// Play card.
					mana -= card.cost;
					const uint32_t target = enemyResult == -1 ? allyResult : enemyResult;

					ActionState cardPlayActionState{};
					cardPlayActionState.trigger = ActionState::Trigger::onCardPlayed;
					cardPlayActionState.src = selectedId;
					cardPlayActionState.dst = target + (enemyResult != -1) * BOARD_CAPACITY_PER_SIDE;
					cardPlayActionState.dstUniqueId = boardState.uniqueIds[cardPlayActionState.dst];
					cardPlayActionState.source = ActionState::Source::hand;
					state.stack.Add() = cardPlayActionState;
				}
			}

			selectionState = SelectionState::none;
		}
			
		return true;
	}

	bool MainLevel::CombatState::PreHandleActionState(State& state, const LevelUpdateInfo& info,
		ActionState& actionState)
	{
		if (!ValidateActionState(state, actionState))
			return false;

		auto& boardState = state.boardState;
		if(actionState.trigger == ActionState::Trigger::onStartOfTurn)
		{
			previousEventCard = eventCard;
			eventCard = state.GetEvent(info);
		}
		else if (actionState.trigger == ActionState::Trigger::onSummon)
		{
			constexpr auto vIsAlly = static_cast<uint32_t>(ActionState::VSummon::isAlly);
			constexpr auto vId = static_cast<uint32_t>(ActionState::VSummon::id);
			constexpr auto vPartyId = static_cast<uint32_t>(ActionState::VSummon::partyId);
			constexpr auto vHealth = static_cast<uint32_t>(ActionState::VSummon::health);

			const bool isAlly = actionState.values[vIsAlly] == 1;
			const auto monsterId = actionState.values[vId];
			const auto partyId = isAlly ? actionState.values[vPartyId] : -1;
			const auto health = actionState.values[vHealth];

			const uint32_t targetId = isAlly ? boardState.allyCount : BOARD_CAPACITY_PER_SIDE + boardState.enemyCount;
			boardState.ids[targetId] = monsterId;
			boardState.combatStats[targetId] = GetCombatStat(info.monsters[monsterId]);
			boardState.uniqueIds[targetId] = uniqueId++;
			boardState.partyIds[targetId] = partyId;
			if (health != -1)
				boardState.combatStats[targetId].health = health;
			if (isAlly)
				++boardState.allyCount;
			else
				++boardState.enemyCount;
		}
		else if (actionState.trigger == ActionState::Trigger::draw)
			state.hand.Add() = state.Draw(info);

		return true;
	}

	bool MainLevel::CombatState::PostHandleActionState(State& state, const LevelUpdateInfo& info,
		ActionState& actionState)
	{
		if (!ValidateActionState(state, actionState))
			return false;

		auto& boardState = state.boardState;
		const auto& playerState = info.playerState;

		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const auto partyId = boardState.partyIds[i];

			if (partyId == -1)
				break;

			for (uint32_t j = 0; j < playerState.artifactSlotCounts[partyId]; ++j)
			{
				const uint32_t id = info.playerState.artifacts[partyId];
				if (id == -1)
					continue;
				const auto& artifact = info.artifacts[id];
				if (artifact.onActionEvent)
					artifact.onActionEvent(state, actionState, i);
			}
		}

		const auto& path = state.paths[state.GetPrimaryPath()];
		const auto& room = info.rooms[path.room];
		if (room.onActionEvent)
			room.onActionEvent(state, actionState, 0);
		if(eventCard != -1)
		{
			const auto& event = info.events[eventCard];
			if (event.onActionEvent)
				event.onActionEvent(state, actionState, 0);
		}

		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const auto id = boardState.ids[i];
			const auto& monster = info.monsters[id];
			if (monster.onActionEvent)
				monster.onActionEvent(state, actionState, i);
		}
		for (uint32_t i = 0; i < boardState.enemyCount; ++i)
		{
			const auto id = boardState.ids[BOARD_CAPACITY_PER_SIDE + i];
			const auto& monster = info.monsters[id];
			if (monster.onActionEvent)
				monster.onActionEvent(state, actionState, i);
		}
		for (uint32_t i = 0; i < state.hand.count; ++i)
		{
			const auto& magic = &info.magics[state.hand[i]];
			if (magic->onActionEvent)
				magic->onActionEvent(state, actionState, i);
		}

		if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
		{
			// Set new random enemy targets.
			for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				targets[i] = boardState.allyCount == 0 ? -1 : rand() % boardState.allyCount;

			// Untap.
			for (auto& b : tapped)
				b = false;

			maxMana = jv::Min(maxMana + 1, MAX_MANA);;
			mana = maxMana;

			ActionState drawState{};
			drawState.trigger = ActionState::Trigger::draw;
			state.stack.Add() = drawState;
		}
		else if (actionState.trigger == ActionState::Trigger::onAttack)
		{
			auto& combatStats = boardState.combatStats[actionState.dst];
			auto attack = boardState.combatStats[actionState.src].attack;

			if (actionState.src < BOARD_CAPACITY_PER_SIDE)
			{
				tapped[actionState.src] = true;
				auto& target = targets[actionState.dst - BOARD_CAPACITY_PER_SIDE];
				const auto oldTarget = target;
				if (boardState.allyCount > 1)
				{
					while (target == oldTarget)
						target = rand() % boardState.allyCount;
				}
			}

			const uint32_t roll = rand() % 6;

			if (roll == 6 || roll >= combatStats.armorClass)
			{
				ActionState damageState = actionState;
				damageState.trigger = ActionState::Trigger::onDamage;
				constexpr auto vDamage = static_cast<uint32_t>(ActionState::VDamage::damage);
				damageState.values[vDamage] = attack;
				state.stack.Add() = damageState;
			}
			else
			{
				ActionState missState = actionState;
				missState.trigger = ActionState::Trigger::onMiss;
				state.stack.Add() = missState;
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onDamage)
		{
			auto& combatStats = boardState.combatStats[actionState.dst];
			auto& health = combatStats.health;

			if (health > 0)
			{
				constexpr auto vDamage = static_cast<uint32_t>(ActionState::VDamage::damage);
				health = health < actionState.values[vDamage] ? 0 : health - actionState.values[vDamage];

				if (health == 0)
				{
					ActionState deathActionState = actionState;
					deathActionState.trigger = ActionState::Trigger::onDeath;
					state.stack.Add() = deathActionState;
				}
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onDeath)
		{
			uint32_t i = actionState.dst;
			const bool isEnemy = i >= BOARD_CAPACITY_PER_SIDE;
			const uint32_t mod = BOARD_CAPACITY_PER_SIDE * isEnemy;
			i -= mod;
			const uint32_t c = isEnemy ? boardState.enemyCount : boardState.allyCount;

			if (isEnemy)
			{
				for (uint32_t j = i; j < c; ++j)
					targets[j] = targets[j + 1];
				lastEnemyDefeatedId = boardState.ids[i + mod];
				--boardState.enemyCount;
			}
			else
			{
				for (uint32_t j = i; j < c; ++j)
					boardState.partyIds[j + mod] = boardState.partyIds[j + 1 + mod];
				--boardState.allyCount;
			}

			// Remove shared data.
			for (uint32_t j = i; j < c; ++j)
			{
				boardState.ids[j + mod] = boardState.ids[j + 1 + mod];
				boardState.combatStats[j + mod] = boardState.combatStats[j + 1 + mod];
				boardState.uniqueIds[j + mod] = boardState.uniqueIds[j + 1 + mod];
			}

			// Modify states as to ensure other events can go through.
			for (auto& as : state.stack)
			{
				// Invalidate everything that attacks this.
				if (as.src == i || as.dst == i)
				{
					as.src = -1;
					as.dst = -1;
				}

				if (isEnemy)
				{
					if (as.src != -1 && as.source == ActionState::Source::board && as.src > actionState.src)
						--as.src;
					if (as.dst != -1 && as.dst > actionState.dst)
						--as.dst;
				}
				else
				{
					if (as.src != -1 && as.source == ActionState::Source::board && as.src > actionState.src && as.src < BOARD_CAPACITY_PER_SIDE)
						--as.src;
					if (as.dst != -1 && as.dst > actionState.dst && as.dst < BOARD_CAPACITY_PER_SIDE)
						--as.dst;
				}
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onCardPlayed)
			state.hand.RemoveAtOrdered(actionState.src);

		return true;
	}

	bool MainLevel::CombatState::ValidateActionState(const State& state, ActionState& actionState)
	{
		const auto& boardState = state.boardState;

		const bool handSrc = actionState.source == ActionState::Source::hand;
		bool validActionState;
		bool validSrc;
		bool validDst;

		switch (actionState.trigger)
		{
		case ActionState::Trigger::onDamage:
		case ActionState::Trigger::onAttack:
		case ActionState::Trigger::onMiss:
		case ActionState::Trigger::onDeath:
			validSrc = handSrc || actionState.src != -1 && state.boardState.uniqueIds[actionState.src] == actionState.srcUniqueId;
			validDst = actionState.dst != -1 && state.boardState.uniqueIds[actionState.dst] == actionState.dstUniqueId;
			validSrc = !validSrc ? false : handSrc ? true : actionState.src >= BOARD_CAPACITY_PER_SIDE ? 
				actionState.src - BOARD_CAPACITY_PER_SIDE < boardState.enemyCount : actionState.src < boardState.allyCount;
			validDst = !validDst ? false : actionState.dst >= BOARD_CAPACITY_PER_SIDE ?
				actionState.dst - BOARD_CAPACITY_PER_SIDE < boardState.enemyCount : actionState.dst < boardState.allyCount;
			validActionState = validSrc && validDst;
			break;
		case ActionState::Trigger::onSummon:
			validActionState = actionState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] == 1 ?
				boardState.allyCount < BOARD_CAPACITY_PER_SIDE : boardState.enemyCount < BOARD_CAPACITY_PER_SIDE;
			break;
		case ActionState::Trigger::draw:
			validActionState = state.hand.count < state.hand.length;
			break;
		default:
			validActionState = true;
		}

		return validActionState;
	}

	void MainLevel::CombatState::DrawAttackAnimation(const State& state, const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeState)
			return;
		const auto& actionState = state.stack.Peek();
		if (actionState.trigger != ActionState::Trigger::onAttack)
			return;

		const auto& boardState = state.boardState;
		assert(actionState.trigger == ActionState::Trigger::onAttack);

		uint32_t src = actionState.src;
		uint32_t dst = actionState.dst;

		if (src >= BOARD_CAPACITY_PER_SIDE)
			src -= BOARD_CAPACITY_PER_SIDE;
		else if (dst >= BOARD_CAPACITY_PER_SIDE)
			dst -= BOARD_CAPACITY_PER_SIDE;
		
		const uint32_t c = allied ? boardState.allyCount : boardState.enemyCount;
		const uint32_t oC = allied ? boardState.enemyCount : boardState.allyCount;
		const bool isSrc = allied ? actionState.src < BOARD_CAPACITY_PER_SIDE : actionState.src >= BOARD_CAPACITY_PER_SIDE;

		const float ind = static_cast<float>(isSrc ? src : dst);
		const float offset = ind - static_cast<float>(c - 1) / 2;

		const float oInd = static_cast<float>(isSrc ? dst : src);
		const float oOffset = oInd - static_cast<float>(oC - 1) / 2;

		constexpr float moveDuration = ATTACK_ACTION_STATE_DURATION / 4;
		const float t = level.GetTime();

		const auto curve = je::CreateCurveOvershooting();
		const auto curveDown = je::CreateCurveDecelerate();

		const float l = GetActionStateLerp(level, ATTACK_ACTION_STATE_DURATION);
		const float hEval = DoubleCurveEvaluate(l, curve, curveDown);
		// Move towards each other.
		const float delta = (offset - oOffset) / 2;
		const float tOff = jv::Min(hEval * (1.f / moveDuration), 1.f);
		const float xS = GetCardShape(info, drawInfo).x;
		drawInfo.centerOffset += -tOff * delta * xS;

		const auto cardShape = GetCardShape(info, drawInfo);

		// Move towards dst.
		if(isSrc && l >= moveDuration)
		{
			const float vEval = DoubleCurveEvaluate(l, curve, curveDown);
			const float tMOff = jv::Min((l - moveDuration) * (1.f / moveDuration) * vEval, 1.f);
			drawInfo.overridePosIndex = src;
			drawInfo.overridePos = GetCardPosition(info, drawInfo, src);
			drawInfo.overridePos.y += tMOff * (ENEMY_HEIGHT - ALLY_HEIGHT - cardShape.y) * (2 * allied - 1);
		}
		else if(!isSrc && l >= moveDuration * 2)
		{
			const float vEval = DoubleCurveEvaluate(l, curveDown, curve);
			const float tMOff = jv::Min((t - timeSinceLastActionState - ATTACK_ACTION_STATE_DURATION / 2) * 2 * vEval, 1.f);
			drawInfo.overridePosIndex = dst;
			drawInfo.overridePos = GetCardPosition(info, drawInfo, dst);
			drawInfo.overridePos.y += tMOff * cardShape.x * (2 * !allied - 1);
		}
	}

	void MainLevel::CombatState::DrawDamageAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeState)
			return;
		if (allied && activeState->dst >= BOARD_CAPACITY_PER_SIDE || !allied && activeState->dst < BOARD_CAPACITY_PER_SIDE)
			return;
		const auto damageTrigger = activeState->trigger == ActionState::Trigger::onDamage;
		const auto missTrigger = activeState->trigger == ActionState::Trigger::onMiss;
		if (!damageTrigger && !missTrigger)
			return;

		const auto pos = GetCardPosition(info, drawInfo, activeState->dst - !allied * BOARD_CAPACITY_PER_SIDE);

		TextTask textTask{};
		textTask.center = true;
		textTask.position = pos;
		textTask.text = "miss";
		textTask.priority = true;

		if(damageTrigger)
		{
			const uint32_t dmg = activeState->values[static_cast<uint32_t>(ActionState::VDamage::damage)];
			textTask.text = TextInterpreter::IntToConstCharPtr(dmg, info.frameArena);
			textTask.color = glm::vec4(1, 0, 0, 1);
			textTask.text = TextInterpreter::Concat("-", textTask.text, info.frameArena);

			drawInfo.damagedIndex = activeState->dst - !allied * BOARD_CAPACITY_PER_SIDE;
		}
		
		const auto curve = je::CreateCurveOvershooting();
		const float eval = curve.Evaluate(GetActionStateLerp(level));

		textTask.position.y += eval * 24;

		info.textTasks.Push(textTask);
	}

	void MainLevel::CombatState::DrawSummonAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeState)
			return;
		if (activeState->trigger != ActionState::Trigger::onSummon)
			return;
		if (allied != activeState->values[static_cast<uint32_t>(ActionState::VSummon::isAlly)])
			return;

		DrawDrawAnimation(info, level, drawInfo);
	}

	void MainLevel::CombatState::DrawDrawAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo) const
	{
		if (!activeState)
			return;
		
		drawInfo.spawning = true;
		drawInfo.spawnLerp = GetActionStateLerp(level);
	}

	void MainLevel::CombatState::DrawDeathAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeState)
			return;
		if (activeState->trigger != ActionState::Trigger::onDeath)
			return;
		if (allied && activeState->dst >= BOARD_CAPACITY_PER_SIDE || !allied && activeState->dst < BOARD_CAPACITY_PER_SIDE)
			return;
		
		drawInfo.dyingIndex = activeState->dst;
		if (activeState->dst >= BOARD_CAPACITY_PER_SIDE)
			drawInfo.dyingIndex -= BOARD_CAPACITY_PER_SIDE;
		drawInfo.dyingLerp = GetActionStateLerp(level);;
	}

	void MainLevel::CombatState::DrawCardPlayAnimation(const Level& level, CardSelectionDrawInfo& drawInfo) const
	{
		if (!activeState)
			return;
		if (activeState->trigger != ActionState::Trigger::onCardPlayed)
			return;
		DrawFadeAnimation(level, drawInfo, activeState->src);
	}

	void MainLevel::CombatState::DrawFadeAnimation(const Level& level, CardSelectionDrawInfo& drawInfo, const uint32_t src) const
	{
		if (!activeState)
			return;
		drawInfo.dyingIndex = src;
		drawInfo.dyingLerp = GetActionStateLerp(level);
	}

	float MainLevel::CombatState::GetActionStateLerp(const Level& level, const float duration) const
	{
		const float t = level.GetTime();
		const float aTime = t - timeSinceLastActionState;
		return (duration - (duration - aTime)) / duration;
	}

	void MainLevel::RewardMagicCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
	}

	bool MainLevel::RewardMagicCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[MAGIC_DECK_SIZE];
		uint32_t costs[MAGIC_DECK_SIZE];
		for (uint32_t i = 0; i < MAGIC_DECK_SIZE; ++i)
		{
			const auto c = &info.magics[info.gameState.magics[i]];
			cards[i] = c;
			costs[i] = c->cost;
		}

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y / 2 + 40;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.length = MAGIC_DECK_SIZE;
		cardSelectionDrawInfo.costs = costs;
		cardSelectionDrawInfo.hoverDurations = hoverDurations;
		const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		const auto& path = state.paths[state.chosenPath];

		const auto rewardCard = &info.magics[path.magic];
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = rewardCard;
		cardDrawInfo.center = true;
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y / 2 + 44 };
		cardDrawInfo.lifeTime = level->GetTime();
		cardDrawInfo.cost = rewardCard->cost;
		cardDrawInfo.hoverDuration = &hoverDurations[MAGIC_DECK_SIZE];
		level->DrawCard(info, cardDrawInfo);

		if (info.inputState.lMouse.PressEvent())
			discoverOption = choice == discoverOption ? -1 : choice;

		const char* text = "select a card to replace, if any.";
		level->DrawTopCenterHeader(info, HeaderSpacing::normal, text);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, f);
		
		if (info.inputState.enter.PressEvent())
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
		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
	}

	bool MainLevel::RewardFlawCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		bool greyedOut[PARTY_ACTIVE_CAPACITY];
		bool flawSlotAvailable = false;

		for (uint32_t i = 0; i < gameState.partyCount; ++i)
		{
			const auto& flaw = info.gameState.flaws[i];
			greyedOut[i] = flaw != -1 || gameState.partyIds[i] == -1;
			flawSlotAvailable = flawSlotAvailable ? true : flaw == -1;
		}

		if (flawSlotAvailable)
		{
			level->DrawTopCenterHeader(info, HeaderSpacing::normal, "select the bearer of this curse.");

			Card* cards[PARTY_ACTIVE_CAPACITY];
			CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
			for (uint32_t i = 0; i < gameState.partyCount; ++i)
			{
				const auto monster = &info.monsters[gameState.monsterIds[i]];
				cards[i] = monster;
				combatStats[i] = GetCombatStat(*monster);
			}

			Card** stacks[PARTY_CAPACITY]{};
			uint32_t stackCounts[PARTY_CAPACITY]{};

			uint32_t ogPartyCount = 0;
			for (uint32_t i = 0; i < gameState.partyCount; ++i)
			{
				const auto partyId = gameState.partyIds[i];
				if (partyId == -1)
					break;
				++ogPartyCount;

				const uint32_t count = stackCounts[i] = playerState.artifactSlotCounts[partyId];
				if (count == 0)
					continue;

				const auto arr = jv::CreateArray<Card*>(info.frameArena, count + 1);
				stacks[i] = arr.ptr;
				for (uint32_t j = 0; j < count; ++j)
				{
					const uint32_t index = playerState.artifacts[partyId * MONSTER_ARTIFACT_CAPACITY + j];
					arr[j] = index == -1 ? nullptr : &info.artifacts[index];
				}

				const uint32_t flaw = info.gameState.flaws[i];
				if (flaw != -1)
				{
					++stackCounts[i];
					stacks[i][count] = &info.flaws[flaw];
				}
			}

			CardSelectionDrawInfo cardSelectionDrawInfo{};
			cardSelectionDrawInfo.cards = cards;
			cardSelectionDrawInfo.length = ogPartyCount;
			cardSelectionDrawInfo.greyedOutArr = greyedOut;
			cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
			cardSelectionDrawInfo.stacks = stacks;
			cardSelectionDrawInfo.stackCounts = stackCounts;
			cardSelectionDrawInfo.highlighted = discoverOption;
			cardSelectionDrawInfo.combatStats = combatStats;
			cardSelectionDrawInfo.hoverDurations = hoverDurations;
			const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
			
			const auto& path = state.paths[state.chosenPath];

			CardDrawInfo cardDrawInfo{};
			cardDrawInfo.card = &info.flaws[path.flaw];
			cardDrawInfo.center = true;
			cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
			cardDrawInfo.lifeTime = level->GetTime();
			cardDrawInfo.hoverDuration = &hoverDurations[PARTY_ACTIVE_CAPACITY];
			level->DrawCard(info, cardDrawInfo);

			if (info.inputState.lMouse.PressEvent())
			{
				discoverOption = choice == discoverOption ? -1 : choice;
				if (discoverOption != -1)
					timeSinceDiscovered = level->GetTime();
			}

			if (discoverOption == -1)
				return true;
			
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, level->GetTime() - timeSinceDiscovered);
			if (!info.inputState.enter.PressEvent())
				return true;

			info.gameState.flaws[discoverOption] = path.flaw;
		}

		stateIndex = static_cast<uint32_t>(StateNames::exitFound);
		return true;
	}

	void MainLevel::RewardArtifactState::Reset(State& state, const LevelInfo& info)
	{
		for (auto& hoverDuration : hoverDurations)
			hoverDuration = 0;
		if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
		{
			auto& playerState = info.playerState;
			const auto& gameState = info.gameState;

			for (uint32_t i = 0; i < gameState.partyCount; ++i)
			{
				if (gameState.partyIds[i] == -1)
					break;

				const uint32_t id = gameState.partyIds[i];
				auto& slotCount = playerState.artifactSlotCounts[id];
				slotCount = jv::Max(slotCount, state.depth / ROOM_COUNT_BEFORE_BOSS);
			}
		}
	}

	bool MainLevel::RewardArtifactState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		Card* cards[PARTY_ACTIVE_CAPACITY]{};
		
		auto& playerState = info.playerState;
		const auto& gameState = info.gameState;

		const char* text = "you have the chance to equip this new artifact, and swap your artifacts around.";
		level->DrawTopCenterHeader(info, HeaderSpacing::normal, text);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, f);

		for (uint32_t i = 0; i < gameState.partyCount; ++i)
		{
			if (gameState.partyIds[i] == -1)
				break;
			cards[i] = &info.monsters[playerState.monsterIds[gameState.partyIds[i]]];
		}
		
		Card** artifacts[PARTY_ACTIVE_CAPACITY]{};
		CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};

		uint32_t ogPartyCount = 0;
		for (uint32_t i = 0; i < gameState.partyCount; ++i)
		{
			const auto partyId = gameState.partyIds[i];
			if (partyId == -1)
				break;
			++ogPartyCount;
			
			const auto monster = &info.monsters[playerState.monsterIds[partyId]];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
			combatStats[i].health = gameState.healths[i];

			const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[partyId];
			if (count == 0)
				continue;
			const auto arr = artifacts[i] = jv::CreateArray<Card*>(info.frameArena, MONSTER_ARTIFACT_CAPACITY).ptr;
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = playerState.artifacts[partyId * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		uint32_t outStackSelected;
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = ogPartyCount;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
		cardSelectionDrawInfo.stacks = artifacts;
		cardSelectionDrawInfo.stackCounts = artifactCounts;
		cardSelectionDrawInfo.outStackSelected = &outStackSelected;
		cardSelectionDrawInfo.combatStats = combatStats;
		cardSelectionDrawInfo.hoverDurations = hoverDurations;
		const auto choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		auto& path = state.paths[state.chosenPath];
		
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
		cardDrawInfo.center = true;
		cardDrawInfo.lifeTime = level->GetTime();
		cardDrawInfo.hoverDuration = &hoverDurations[PARTY_ACTIVE_CAPACITY];
		level->DrawCard(info, cardDrawInfo);
		
		if(choice != -1 && outStackSelected != -1)
		{
			const uint32_t id = gameState.partyIds[choice];

			if (info.inputState.lMouse.PressEvent())
			{
				const uint32_t swappable = path.artifact;
				path.artifact = playerState.artifacts[id * MONSTER_ARTIFACT_CAPACITY + outStackSelected];
				playerState.artifacts[id * MONSTER_ARTIFACT_CAPACITY + outStackSelected] = swappable;
			}
		}

		if (info.inputState.enter.PressEvent())
			stateIndex = static_cast<uint32_t>(StateNames::exitFound);

		return true;
	}

	void MainLevel::ExitFoundState::Reset(State& state, const LevelInfo& info)
	{
		LevelState<State>::Reset(state, info);
		managingParty = false;
		for (auto& b : selected)
			b = false;
		timeSincePartySelected = -1;

		auto& gameState = info.gameState;

		// Move flaws.
		for (int32_t i = static_cast<int32_t>(state.boardState.allyCount) - 1; i >= 0; --i)
		{
			if (i >= PARTY_ACTIVE_CAPACITY)
				continue;
			const auto partyId = state.boardState.partyIds[i];
			if (partyId == -1)
				gameState.flaws[i] = -1;
			if (partyId != gameState.partyIds[i])
			{
				for (uint32_t j = i; j < state.boardState.allyCount - 1; ++j)
					gameState.flaws[j] = gameState.flaws[j + 1];
				++i;
			}
		}
	}

	bool MainLevel::ExitFoundState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& gameState = info.gameState;
		auto& playerState = info.playerState;

		if(managingParty)
		{
			level->DrawTopCenterHeader(info, HeaderSpacing::close, "your party is too big. choose some to say goodbye to.");

			Card* cards[PARTY_CAPACITY + PARTY_ACTIVE_CAPACITY];
			Card** artifacts[PARTY_CAPACITY + PARTY_ACTIVE_CAPACITY]{};
			CombatStats combatStats[PARTY_CAPACITY + PARTY_ACTIVE_CAPACITY];
			uint32_t artifactCounts[PARTY_CAPACITY + PARTY_ACTIVE_CAPACITY]{};
			
			for (uint32_t i = 0; i < playerState.partySize; ++i)
			{
				const auto monster = &info.monsters[playerState.monsterIds[i]];
				cards[i] = monster;
				combatStats[i] = GetCombatStat(*monster);

				const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[i];
				if (count == 0)
					continue;

				const auto arr = artifacts[i] = jv::CreateArray<Card*>(info.frameArena, MONSTER_ARTIFACT_CAPACITY).ptr;
				for (uint32_t j = 0; j < count; ++j)
				{
					const uint32_t index = playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
					arr[j] = index == -1 ? nullptr : &info.artifacts[index];
				}
			}

			uint32_t c = playerState.partySize;
			for (uint32_t i = 0; i < gameState.partyCount; ++i)
			{
				if (gameState.partyIds[i] != -1)
					continue;
				const auto monster = &info.monsters[gameState.monsterIds[i]];
				combatStats[c] = GetCombatStat(*monster);
				cards[c++] = monster;
			}

			uint32_t outStackSelected;
			CardSelectionDrawInfo cardSelectionDrawInfo{};
			cardSelectionDrawInfo.cards = cards;
			cardSelectionDrawInfo.length = c;
			cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
			cardSelectionDrawInfo.stacks = artifacts;
			cardSelectionDrawInfo.stackCounts = artifactCounts;
			cardSelectionDrawInfo.outStackSelected = &outStackSelected;
			cardSelectionDrawInfo.combatStats = combatStats;
			cardSelectionDrawInfo.selectedArr = selected;
			const auto choice = level->DrawCardSelection(info, cardSelectionDrawInfo);

			if (info.inputState.lMouse.PressEvent() && choice != -1)
				selected[choice] = !selected[choice];

			uint32_t remaining = c;
			for (uint32_t i = 0; i < c; ++i)
				remaining -= selected[i];

			if (remaining > 0 && remaining <= PARTY_CAPACITY)
			{
				if (timeSincePartySelected < 0)
					timeSincePartySelected = level->GetTime();

				level->DrawPressEnterToContinue(info, HeaderSpacing::normal, level->GetTime() - timeSincePartySelected);
				if (info.inputState.enter.PressEvent())
				{
					uint32_t d = 0;
					for (uint32_t i = 0; i < playerState.partySize; ++i)
					{
						if (!selected[i])
						{
							for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
							{
								playerState.artifacts[j + d * MONSTER_ARTIFACT_CAPACITY] = playerState.artifacts[j * i * MONSTER_ARTIFACT_CAPACITY];
								playerState.artifactSlotCounts[d] = playerState.artifactSlotCounts[i];
							}
							playerState.monsterIds[d++] = playerState.monsterIds[i];
						}
						else
							playerState.artifactSlotCounts[i] = 0;
					}
					
					for (uint32_t i = 0; i < gameState.partyCount; ++i)
					{
						if (gameState.partyIds[i] == -1 && !selected[playerState.partySize + i])
							playerState.monsterIds[d++] = gameState.monsterIds[i];
					}

					playerState.partySize = remaining;

					SaveData(playerState);
					loadLevelIndex = LevelIndex::mainMenu;
				}
			}
			else
				timeSincePartySelected = -1;
			
			return true;
		}

		level->DrawTopCenterHeader(info, HeaderSpacing::close, "press onwards, or flee.");

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + 9 };
		buttonDrawInfo.text = "continue";
		buttonDrawInfo.center = true;

		if (level->DrawButton(info, buttonDrawInfo))
		{
			stateIndex = static_cast<uint32_t>(state.depth % ROOM_COUNT_BEFORE_BOSS == 0 ? StateNames::bossReveal : StateNames::pathSelect);
			return true;
		}

		buttonDrawInfo.origin.y -= 18;
		buttonDrawInfo.text = "quit";
		if (level->DrawButton(info, buttonDrawInfo))
		{
			for (uint32_t i = 0; i < gameState.partyCount; ++i)
			{
				const auto partyId = gameState.partyIds[i];
				if (partyId == -1)
				{
					if(playerState.partySize < PARTY_CAPACITY)
						playerState.monsterIds[playerState.partySize++] = gameState.monsterIds[i];
					else
					{
						managingParty = true;
						return true;
					}
				}
			}

			SaveData(playerState);
			loadLevelIndex = LevelIndex::mainMenu;
			return true;
		}
		
		return true;
	}

	void MainLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);

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
		if (!Level::Update(info, loadLevelIndex))
			return false;

		return stateMachine.Update(info, this, loadLevelIndex);
	}
}
