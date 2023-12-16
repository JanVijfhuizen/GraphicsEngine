#include "pch_game.h"
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
#include "Utils/SubTextureUtils.h"

namespace game
{
	void MainLevel::BossRevealState::Reset(State& state, const LevelInfo& info)
	{
		uint32_t i = 0;
		for (auto& path : state.paths)
		{
			path = {};
			if (state.depth < SUB_BOSS_COUNT * ROOM_COUNT_BEFORE_BOSS)
				path.boss = state.GetBoss(info, i);
			if(path.boss == -1)
				path.boss = MONSTER_IDS::GOD;
			++i;
		}
		for (auto& metaData : metaDatas)
			metaData = {};
	}

	bool MainLevel::BossRevealState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		return true;
	}

	void MainLevel::PathSelectState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;

		const bool addFlaw = state.depth % 2 == 1;

		for (auto& path : state.paths)
		{
			path.room = state.GetRoom(info);
			path.spell = state.GetSpell(info);
			if (addFlaw)
				path.curse = state.GetCurse(info);
			else
				path.artifact = state.GetArtifact(info);
		}
		for (auto& metaData : metaDatas)
			metaData = {};
		info.gameState.artifactSlotCount = jv::Min<uint32_t>(1 + state.depth / (ROOM_COUNT_BEFORE_BOSS * 2), 3);
	}

	bool MainLevel::PathSelectState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const bool flawPresent = state.depth % 2 == 1;

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

		Card** stacks[DISCOVER_LENGTH]{};
		for (auto& stack : stacks)
			stack = jv::CreateArray<Card*>(info.frameArena, 3).ptr;

		uint32_t stacksCounts[DISCOVER_LENGTH]{};
		for (auto& c : stacksCounts)
			c = 2;

		const uint32_t count = 1 + (state.depth >= ROOMS_BEFORE_ROOM_EFFECTS);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			const auto& stack = stacks[i];
			const auto& path = state.paths[i];

			cards[i] = &info.spells[path.spell];
			stack[0] = &info.rooms[path.room];

			if (flawPresent)
				stack[count - 1] = &info.curses[path.curse];
			else
				stack[count - 1] = &info.artifacts[path.artifact];
			stacksCounts[i] = count;
		}
		
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = DISCOVER_LENGTH;
		cardSelectionDrawInfo.stacks = stacks;
		cardSelectionDrawInfo.stackCounts = stacksCounts;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.metaDatas = metaDatas;
		cardSelectionDrawInfo.offsetMod += 12;
		cardSelectionDrawInfo.lifeTime = level->GetTime();
		uint32_t selected = level->DrawCardSelection(info, cardSelectionDrawInfo);

		if (!level->GetIsLoading() && info.inputState.lMouse.PressEvent())
		{
			discoverOption = selected == discoverOption ? -1 : selected;
			if (selected != -1)
				timeSinceDiscovered = level->GetTime();
		}

		const char* text = "select a path.";
		level->DrawTopCenterHeader(info, HeaderSpacing::close, text);
		
		if (discoverOption != -1)
		{
			level->DrawPressEnterToContinue(info, HeaderSpacing::close, level->GetTime() - timeSinceDiscovered);
			
			if (!level->GetIsLoading() && info.inputState.enter.PressEvent())
			{
				state.chosenPath = discoverOption;
				auto& path = state.paths[discoverOption];
				++path.counters;

				stateIndex = static_cast<uint32_t>(StateNames::combat);
			}
		}

		return true;
	}

	void MainLevel::CombatState::Reset(State& state, const LevelInfo& info)
	{
		const bool bossPresent = (state.depth + 1) % ROOM_COUNT_BEFORE_BOSS == 0;
		const auto& gameState = info.gameState;

		for (auto& metaData : metaDatas)
			metaData = {};

		state.mana = 0;
		state.maxMana = 0;
		state.turn = 0;
		lastEnemyDefeatedId = -1;

		activations = {};
		activations.ptr = activationsPtr;
		activations.length = 2 + HAND_MAX_SIZE + BOARD_CAPACITY;

		recruitSceneLifetime = -1;
		selectionState = SelectionState::none;
		time = 0;
		selectedId = -1;
		uniqueId = 0;
		timeSinceLastActionState = 0;
		activeState = {};
		activeStateValid = false;
		comboCounter = 0;
		timeSinceStackOverloaded = -1;
		timeSinceEmptyStack = -1;

		for (auto& card : eventCards)
			card = -1;
		for (auto& card : previousEventCards)
			card = -1;

		state.boardState = {};
		state.hand.Clear();
		state.stack.Clear();
		state.ResetDeck(info);
		for (bool& b : state.tapped)
			b = false;

		ActionState startOfTurnActionState{};
		startOfTurnActionState.trigger = ActionState::Trigger::onStartOfTurn;
		state.TryAddToStack(startOfTurnActionState);

		if (!bossPresent)
		{
			const auto enemyCount = jv::Min<uint32_t>(jv::Min<uint32_t>(2 + state.depth / (ROOM_COUNT_BEFORE_BOSS * 2), 6), state.depth + 1);
			for (uint32_t i = 0; i < enemyCount; ++i)
			{
				const uint32_t id = state.GetMonster(info);
				const uint32_t healthMod = (state.depth / ROOM_COUNT_BEFORE_BOSS) * 5;

				uint32_t health = info.monsters[id].health + healthMod;
				health = jv::Min<uint32_t>(health, 50);

				ActionState summonState{};
				summonState.trigger = ActionState::Trigger::onSummon;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = id;
				summonState.values[static_cast<uint32_t>(ActionState::VSummon::health)] = health;
				state.TryAddToStack(summonState);
			}
		}
		else
		{
			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 0;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = state.paths[state.GetPrimaryPath()].boss;
			state.TryAddToStack(summonState);
		}
		
		for (int32_t i = static_cast<int32_t>(gameState.partySize) - 1; i >= 0; --i)
		{
			const auto partyId = gameState.partyIds[i];
			const auto monsterId = gameState.monsterIds[i];

			ActionState summonState{};
			summonState.trigger = ActionState::Trigger::onSummon;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)] = 1;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::id)] = monsterId;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::partyId)] = partyId;
			summonState.values[static_cast<uint32_t>(ActionState::VSummon::health)] = gameState.healths[i];
			state.TryAddToStack(summonState);
		}

		for (uint32_t i = 0; i < HAND_INITIAL_SIZE; ++i)
		{
			ActionState drawState{};
			drawState.trigger = ActionState::Trigger::onDraw;
			state.TryAddToStack(drawState);
		}
	}

	bool MainLevel::CombatState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		time += info.deltaTime;

		auto& gameState = info.gameState;
		auto& boardState = state.boardState;
		
		const bool stackOverloaded = state.stack.count == state.stack.length;
		if (state.stack.count == 0)
		{
			info.activePlayer = false;
			info.inCombat = true;
			comboCounter = 0;
		}
		while(!stackOverloaded && !activeStateValid && state.stack.count > 0)
		{
			timeSinceLastActionState = level->GetTime();
			actionStateDuration = ACTION_STATE_DEFAULT_DURATION;
			activeState = state.stack.Pop();
			
			switch (activeState.trigger)
			{
			case ActionState::Trigger::onAttack:
				actionStateDuration = GetAttackMoveDuration(state, activeState) * 2 + CARD_VERTICAL_MOVE_SPEED * 2;
				break;
			case ActionState::Trigger::onStartOfTurn:
				actionStateDuration = START_OF_TURN_ACTION_STATE_DURATION;
				break;
			case ActionState::Trigger::onDraw:
				actionStateDuration = CARD_DRAW_DURATION;
			default:
				break;
			}

			activeStateValid = PreHandleActionState(state, info, activeState);
			activationDuration = -1;
			++comboCounter;
		}

		if(stackOverloaded || comboCounter > STACK_OVERLOAD_THRESHOLD)
		{
			state.stack.Clear();
			activeStateValid = false;
			comboCounter = 0;
			timeSinceStackOverloaded = level->GetTime();
		}

		if(timeSinceStackOverloaded > 0 && level->GetTime() - timeSinceStackOverloaded < STACK_OVERLOAD_DURATION)
		{
			TextTask textTask{};
			textTask.position = SIMULATED_RESOLUTION / 2;
			textTask.center = true;
			textTask.text = "stack overloaded";
			textTask.priority = true;
			textTask.color = glm::vec4(1, 0, 0, 1);
			textTask.lifetime = level->GetTime() - timeSinceStackOverloaded;
			textTask.scale = 2;
			info.textTasks.Push(textTask);

			info.screenShakeInfo.fallOfThreshold = 1;
			info.screenShakeInfo.intensity = 2;
			info.screenShakeInfo.remaining = STACK_OVERLOAD_DURATION;
			info.screenShakeInfo.timeOut = 1e4;
		}

		if (activeStateValid)
		{
			if (timeSinceLastActionState + actionStateDuration < level->GetTime())
			{
				if (activationDuration < -1e-5f)
				{
					CollectActivatedCards(state, info, activeState);
					activationDuration = 0;
				}
				if (activations.count > 0)
				{
					if (activationDuration < CARD_ACTIVATION_DURATION)
						activationDuration += info.deltaTime;
					else
					{
						activations.Pop();
						activationDuration = 0;
					}
				}
				else
				{
					PostHandleActionState(state, info, level, activeState);
					activeStateValid = false;
					if (state.stack.count == 0)
						timeSinceEmptyStack = level->GetTime();
				}
			}
		}

		if(activeStateValid && activeState.trigger == ActionState::Trigger::onStartOfTurn)
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
				textTask.text = "turn";
				textTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, CENTER_HEIGHT) + glm::ivec2(off2, 3);
				textTask.scale = 2;
				textTask.center = true;
				textTask.priority = true;

				info.textTasks.Push(textTask);
				textTask.text = TextInterpreter::IntToConstCharPtr(state.turn, info.frameArena);
				textTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, CENTER_HEIGHT) - glm::ivec2(off2, 19);
				info.textTasks.Push(textTask);

				LightTask lightTask{};
				lightTask.intensity = 20;
				lightTask.fallOf = 8;
				lightTask.pos = LightTask::ToLightTaskPos(textTask.position);
				lightTask.pos.z = .2f;
				info.lightTasks.Push(lightTask);
			}

			lineRenderTask.position.x = off;
			++lineRenderTask.position.y;
			info.renderTasks.Push(lineRenderTask);
			lineRenderTask.position.x = -off;
			lineRenderTask.position.y -= 2;
			info.renderTasks.Push(lineRenderTask);
		}

		if(!activeStateValid && state.stack.count == 0)
		{
			// Check for game over.
			if (boardState.allyCount == 0)
			{
				info.inCombat = false;
				loadLevelIndex = LevelIndex::gameOver;
				return true;
			}

			// Check for room victory.
			if (boardState.enemyCount == 0)
			{
				bool actionsRemaining = false;
				ActionState killState{};
				killState.trigger = ActionState::Trigger::onDeath;
				killState.source = ActionState::Source::other;

				// Destroy tokens.
				for (uint32_t i = 0; i < boardState.allyCount; ++i)
				{
					const auto id = boardState.ids[i];
					if(info.monsters[id].tags & TAG_TOKEN)
					{
						killState.dst = i;
						killState.dstUniqueId = boardState.uniqueIds[i];
						state.TryAddToStack(killState);
						actionsRemaining = true;
					}
				}

				// Destroy copies.
				for (uint32_t i = 0; i < boardState.allyCount - 1; ++i)
				{
					const uint32_t id = boardState.ids[i];
					for (uint32_t j = i + 1; j < boardState.allyCount; ++j)
					{
						if(boardState.ids[j] == id)
						{
							killState.dst = j;
							killState.dstUniqueId = boardState.uniqueIds[j];
							state.TryAddToStack(killState);
							actionsRemaining = true;
						}
					}
				}

				if(!actionsRemaining)
				{
					if (boardState.allyCount < PARTY_ACTIVE_INITIAL_CAPACITY)
					{
						const auto monster = &info.monsters[lastEnemyDefeatedId];
						bool valid = !monster->unique;

						// Check if you already own this monster.
						if(valid)
							for (uint32_t i = 0; i < boardState.allyCount; ++i)
								if (lastEnemyDefeatedId == boardState.ids[i])
								{
									valid = false;
									break;
								}

						if (valid)
						{
							info.inCombat = false;
							if (recruitSceneLifetime < -1e-5f)
								recruitSceneLifetime = 0;
							else
								recruitSceneLifetime += info.deltaTime;

							// Recruitment.
							level->DrawTopCenterHeader(info, HeaderSpacing::normal, "someone wants to join your party.", 1, recruitSceneLifetime);
							bool recruitScreenActive = true;
							auto combatStats = GetCombatStat(*monster);

							CardDrawInfo cardDrawInfo{};
							cardDrawInfo.card = monster;
							cardDrawInfo.center = true;
							cardDrawInfo.combatStats = &combatStats;
							cardDrawInfo.origin = SIMULATED_RESOLUTION / 2;
							cardDrawInfo.lifeTime = recruitSceneLifetime;
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
								gameState.partyIds[boardState.allyCount] = -1;
								for (uint32_t i = 0; i < MONSTER_ARTIFACT_CAPACITY; ++i)
									gameState.artifacts[boardState.allyCount * MONSTER_ARTIFACT_CAPACITY + i] = -1;
								++boardState.allyCount;
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

				gameState.partySize = jv::Min(boardState.allyCount, PARTY_ACTIVE_CAPACITY);
				for (uint32_t i = 0; i < gameState.partySize; ++i)
				{
					gameState.monsterIds[i] = boardState.ids[i];
					gameState.healths[i] = jv::Min(boardState.combatStats[i].health, info.monsters[gameState.monsterIds[i]].health);
					auto c = 1 + state.depth / ROOM_COUNT_BEFORE_BOSS;
					c = jv::Min(gameState.artifactSlotCount, MONSTER_ARTIFACT_CAPACITY);
					gameState.artifactSlotCount = jv::Max(gameState.artifactSlotCount, c);
				}

				info.inCombat = false;
				if(state.depth % 2 == 0)
					stateIndex = static_cast<uint32_t>(StateNames::rewardArtifact);
				else
					stateIndex = static_cast<uint32_t>(StateNames::rewardFlaw);
				return true;	
				}
			}
		}

		DrawParallaxBackground(info, false);
		DrawParallaxBackground(info, true);
		
		{
			const float l = jv::Min(1.f, level->GetTime() * 3);
			constexpr uint32_t LINE_POSITION = HAND_HEIGHT + 6;
			PixelPerfectRenderTask lineRenderTask{};
			lineRenderTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;
			lineRenderTask.scale.x = SIMULATED_RESOLUTION.x;
			lineRenderTask.scale.y = LINE_POSITION;
			lineRenderTask.position.y = 0;
			lineRenderTask.position.x = 0;
			lineRenderTask.color.a = l;
			info.renderTasks.Push(lineRenderTask);
			lineRenderTask.color = glm::vec4(0, 0, 0, l);
			lineRenderTask.scale.y -= 2;
			info.renderTasks.Push(lineRenderTask);
		}

		{
			auto cards = jv::CreateVector<Card*>(info.frameArena, 2);
	
			CardSelectionDrawInfo eventSelectionDrawInfo{};
			eventSelectionDrawInfo.lifeTime = level->GetTime();
			eventSelectionDrawInfo.cards = cards.ptr;
			eventSelectionDrawInfo.height = HAND_HEIGHT;
			eventSelectionDrawInfo.metaDatas = &metaDatas[META_DATA_ROOM_INDEX];
			eventSelectionDrawInfo.offsetMod = -4;
			eventSelectionDrawInfo.length = 1;
			eventSelectionDrawInfo.centerOffset = -SIMULATED_RESOLUTION.x / 2 + 32;

			// Draws room.
			if(state.depth >= ROOMS_BEFORE_ROOM_EFFECTS)
			{
				const auto& path = state.paths[state.GetPrimaryPath()];
				cards.Add() = &info.rooms[path.room];
				DrawActivationAnimation(eventSelectionDrawInfo, Activation::room, 0);
				level->DrawCardSelection(info, eventSelectionDrawInfo);
				cards.Clear();
			}

			// Draws events.
			const uint32_t c = GetEventCardCount(state);
			if (c > 0)
			{
				const bool isStartOfTurn = activeStateValid && activeState.trigger == ActionState::Trigger::onStartOfTurn;
				if(isStartOfTurn && previousEventCards[0] != -1)
					cards.Add() = &info.events[previousEventCards[0]];
				if (eventCards[0] != -1)
					cards.Add() = &info.events[eventCards[0]];

				eventSelectionDrawInfo.metaDatas = &metaDatas[META_DATA_EVENT_INDEX];
				eventSelectionDrawInfo.activationIndex = -1;
				eventSelectionDrawInfo.centerOffset *= -1;
				eventSelectionDrawInfo.length = cards.count;

				// Draw additional events.
				/*
				Card* addCards[EVENT_CARD_MAX_COUNT * 2 - 2];
				Card** stacks[2]{ addCards, &addCards[EVENT_CARD_MAX_COUNT - 1] };
				uint32_t stacksCounts[2]{ c - 1, c - 1 };

				if (eventCards[0] != -1)
					for (uint32_t i = 0; i < c - 1; ++i)
					{
						addCards[i] = &info.events[eventCards[i + 1]];
						if (isStartOfTurn && previousEventCards[0] != -1)
							addCards[i + EVENT_CARD_MAX_COUNT - 1] = &info.events[previousEventCards[i + 1]];
					}

				eventSelectionDrawInfo.stacks = stacks;
				eventSelectionDrawInfo.stackCounts = stacksCounts;
				*/
				DrawActivationAnimation(eventSelectionDrawInfo, Activation::event, 0);

				if (isStartOfTurn)
				{
					DrawDrawAnimation(*level, eventSelectionDrawInfo);
					if (cards.count > 1)
						DrawFadeAnimation(*level, eventSelectionDrawInfo, 0);
				}

				level->DrawCardSelection(info, eventSelectionDrawInfo);
			}
		}

		// Check for new turn.
		if(state.stack.count == 0 && !activeStateValid)
		{
			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.origin = glm::ivec2(SIMULATED_RESOLUTION.x - 64, 6);
			buttonDrawInfo.text = "end turn";
			buttonDrawInfo.width = 64;
			buttonDrawInfo.centerText = true;
			const bool endTurn = false;//level->DrawButton(info, buttonDrawInfo, level->GetTime() - timeSinceEmptyStack);

			// Manually end turn.
			if (endTurn || info.inputState.enter.PressEvent())
			{
				ActionState startOfTurnActionState{};
				startOfTurnActionState.trigger = ActionState::Trigger::onStartOfTurn;
				state.TryAddToStack(startOfTurnActionState);

				ActionState endOfTurnActionState{};
				endOfTurnActionState.trigger = ActionState::Trigger::onEndOfTurn;
				state.TryAddToStack(endOfTurnActionState);
				info.activePlayer = true;

				ActionState drawState{};
				drawState.trigger = ActionState::Trigger::onDraw;
				state.TryAddToStack(drawState);

				for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				{
					const uint32_t target = state.targets[i];
					if (boardState.allyCount >= target)
					{
						ActionState attackActionState{};
						attackActionState.trigger = ActionState::Trigger::onAttack;
						attackActionState.src = BOARD_CAPACITY_PER_SIDE + i;
						attackActionState.dst = target;
						attackActionState.srcUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + i];
						attackActionState.dstUniqueId = boardState.uniqueIds[target];
						state.TryAddToStack(attackActionState);
					}
				}

				for (bool& b : state.tapped)
					b = true;
			}
		}

		const auto& lMouse = info.inputState.lMouse;
		const bool lMousePressed = lMouse.PressEvent();
		const bool lMouseReleased = lMouse.ReleaseEvent();

		constexpr uint32_t l = HAND_MAX_SIZE > BOARD_CAPACITY_PER_SIDE ? HAND_MAX_SIZE : BOARD_CAPACITY_PER_SIDE;

		Card* cards[l]{};
		for (uint32_t i = 0; i < boardState.enemyCount; ++i)
			cards[i] = &info.monsters[boardState.ids[BOARD_CAPACITY_PER_SIDE + i]];

		uint32_t targets[BOARD_CAPACITY_PER_SIDE];
		memcpy(targets, state.targets, sizeof(uint32_t) * BOARD_CAPACITY_PER_SIDE);
		for (auto& target : targets)
			++target;

		// Draw enemies.
		CardSelectionDrawInfo enemySelectionDrawInfo{};
		enemySelectionDrawInfo.lifeTime = level->GetTime();
		enemySelectionDrawInfo.cards = cards;
		enemySelectionDrawInfo.length = boardState.enemyCount;
		enemySelectionDrawInfo.height = ENEMY_HEIGHT;
		enemySelectionDrawInfo.costs = targets;
		enemySelectionDrawInfo.highlighted = selectionState == SelectionState::enemy ? selectedId : -1;
		enemySelectionDrawInfo.combatStats = &boardState.combatStats[BOARD_CAPACITY_PER_SIDE];
		enemySelectionDrawInfo.metaDatas = &metaDatas[META_DATA_ENEMY_INDEX];
		enemySelectionDrawInfo.offsetMod = 16;
		enemySelectionDrawInfo.mirrorHorizontal = true;
		enemySelectionDrawInfo.selectable = state.stack.count == 0 && !activeStateValid;

		DrawActivationAnimation(enemySelectionDrawInfo, Activation::monster, BOARD_CAPACITY_PER_SIDE);
		DrawAttackAnimation(state, info, *level, enemySelectionDrawInfo, false);
		DrawDamageAnimation(info, *level, enemySelectionDrawInfo, false);
		DrawSummonAnimation(info, *level, enemySelectionDrawInfo, false);
		DrawBuffAnimation(info, *level, enemySelectionDrawInfo, false);
		DrawDeathAnimation(*level, enemySelectionDrawInfo, false);
		const auto enemyResult = level->DrawCardSelection(info, enemySelectionDrawInfo);

		if (state.stack.count == 0 && lMousePressed && enemyResult != -1)
		{
			selectionState = SelectionState::enemy;
			selectedId = enemyResult;
		}

		bool greyedOutArr[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
			greyedOutArr[i] = info.spells[state.hand[i]].cost > state.mana;

		// Draw hand.
		uint32_t costs[HAND_MAX_SIZE];
		for (uint32_t i = 0; i < state.hand.count; ++i)
		{
			const auto spell = &info.spells[state.hand[i]];
			cards[i] = spell;
			costs[i] = spell->cost;
		}

		CardSelectionDrawInfo handSelectionDrawInfo{};
		handSelectionDrawInfo.lifeTime = level->GetTime();
		handSelectionDrawInfo.cards = cards;
		handSelectionDrawInfo.length = state.hand.count;
		handSelectionDrawInfo.height = HAND_HEIGHT;
		handSelectionDrawInfo.texts = nullptr;
		handSelectionDrawInfo.highlighted = selectionState == SelectionState::hand ? selectedId : -1;
		handSelectionDrawInfo.greyedOutArr = greyedOutArr;
		handSelectionDrawInfo.costs = costs;
		handSelectionDrawInfo.combatStats = nullptr;
		handSelectionDrawInfo.metaDatas = &metaDatas[META_DATA_HAND_INDEX];
		handSelectionDrawInfo.offsetMod = 4;
		handSelectionDrawInfo.selectable = state.stack.count == 0 && !activeStateValid;
		handSelectionDrawInfo.draggable = handSelectionDrawInfo.selectable;
		DrawActivationAnimation(handSelectionDrawInfo, Activation::spell, 0);
		DrawCardPlayAnimation(*level, handSelectionDrawInfo);
		if(activeStateValid && activeState.trigger == ActionState::Trigger::onDraw)
			DrawDrawAnimation(*level, handSelectionDrawInfo);

		const auto handResult = level->DrawCardSelection(info, handSelectionDrawInfo);

		if(state.stack.count == 0 && lMousePressed && handResult != -1)
		{
			selectionState = SelectionState::hand;
			selectedId = handResult;
		}

		// Draw allies.
		Card* playerCards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < boardState.allyCount; ++i)
			playerCards[i] = &info.monsters[boardState.ids[i]];

		Card** stacks[PARTY_CAPACITY]{};
		uint32_t stacksCount[PARTY_CAPACITY]{};

		for (uint32_t i = 0; i < boardState.allyCount; ++i)
		{
			const uint32_t curse = gameState.curses[i];
			const uint32_t artifactCount = gameState.artifactSlotCount;
			const uint32_t count = stacksCount[i] = (curse != -1) + artifactCount;

			if (count == 0)
				continue;
			const auto arr = jv::CreateArray<Card*>(info.frameArena, count);
			stacks[i] = arr.ptr;
			for (uint32_t j = 0; j < artifactCount; ++j)
			{
				const uint32_t index = gameState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
			if(curse != -1)
				stacks[i][artifactCount] = &info.curses[curse];
		}
		
		CardSelectionDrawInfo playerCardSelectionDrawInfo{};
		playerCardSelectionDrawInfo.cards = playerCards;
		playerCardSelectionDrawInfo.length = boardState.allyCount;
		playerCardSelectionDrawInfo.height = ALLY_HEIGHT;
		playerCardSelectionDrawInfo.stacks = stacks;
		playerCardSelectionDrawInfo.stackCounts = stacksCount;
		playerCardSelectionDrawInfo.lifeTime = level->GetTime();
		playerCardSelectionDrawInfo.greyedOutArr = state.tapped;
		playerCardSelectionDrawInfo.combatStats = boardState.combatStats;
		playerCardSelectionDrawInfo.highlighted = selectionState == SelectionState::ally ? selectedId : -1;
		playerCardSelectionDrawInfo.metaDatas = &metaDatas[META_DATA_ALLY_INDEX];
		playerCardSelectionDrawInfo.offsetMod = 16;
		playerCardSelectionDrawInfo.selectable = state.stack.count == 0 && !activeStateValid;
		playerCardSelectionDrawInfo.draggable = playerCardSelectionDrawInfo.selectable;
		DrawActivationAnimation(playerCardSelectionDrawInfo, Activation::monster, 0);
		DrawAttackAnimation(state, info, *level, playerCardSelectionDrawInfo, true);
		DrawDamageAnimation(info, *level, playerCardSelectionDrawInfo, true);
		DrawBuffAnimation(info, *level, playerCardSelectionDrawInfo, true);
		DrawSummonAnimation(info, *level, playerCardSelectionDrawInfo, true);
		DrawDeathAnimation(*level, playerCardSelectionDrawInfo, true);

		const auto allyResult = level->DrawCardSelection(info, playerCardSelectionDrawInfo);
		
		if (state.stack.count == 0 && lMousePressed && allyResult != -1)
		{
			selectionState = SelectionState::ally;
			selectedId = allyResult;
		}

		// Draw mana.
		{
			TextTask manaTextTask{};
			manaTextTask.position = glm::ivec2(SIMULATED_RESOLUTION.x / 2, 8);
			manaTextTask.text = TextInterpreter::IntToConstCharPtr(state.mana, info.frameArena);
			manaTextTask.text = TextInterpreter::Concat(manaTextTask.text, "/", info.frameArena);
			manaTextTask.text = TextInterpreter::Concat(manaTextTask.text, TextInterpreter::IntToConstCharPtr(state.maxMana, info.frameArena), info.frameArena);
			manaTextTask.lifetime = level->GetTime();
			manaTextTask.center = true;
			info.textTasks.Push(manaTextTask);
		}

		if(state.stack.count == 0)
		{
			if(selectionState == SelectionState::ally)
			{
				auto& isTapped = state.tapped[selectedId];
				if (!isTapped)
					level->DrawSelectedCard(&info.monsters[boardState.ids[selectedId]]);
			}
			else if(selectionState == SelectionState::hand)
				level->DrawSelectedCard(&info.spells[state.hand[selectedId]]);
		}

		if (state.stack.count == 0 && lMouseReleased)
		{
			// If player tried to attack an enemy.
			if (selectionState == SelectionState::ally)
			{
				auto& isTapped = state.tapped[selectedId];
				if (!isTapped && enemyResult != -1)
				{
					ActionState attackActionState{};
					attackActionState.trigger = ActionState::Trigger::onAttack;
					attackActionState.src = selectedId;
					attackActionState.dst = BOARD_CAPACITY_PER_SIDE + enemyResult;
					attackActionState.srcUniqueId = boardState.uniqueIds[selectedId];
					attackActionState.dstUniqueId = boardState.uniqueIds[BOARD_CAPACITY_PER_SIDE + enemyResult];
					state.TryAddToStack(attackActionState);
					state.tapped[selectedId] = true;
				}
			}
			else if(selectionState == SelectionState::hand)
			{
				// Do something with the target.
				const auto& card = info.spells[state.hand[selectedId]];
				const bool validAll = card.type == SpellCard::Type::all && info.inputState.mousePos.y > SIMULATED_RESOLUTION.y / 4;
				const bool validTarget = card.type == SpellCard::Type::target && (enemyResult != -1 || allyResult != -1);
				const bool validPlay = validAll || validTarget;

				if(validPlay && card.cost <= state.mana)
				{
					// Play card.
					state.mana -= card.cost;
					const uint32_t target = enemyResult == -1 ? allyResult : enemyResult;

					ActionState cardPlayActionState{};
					cardPlayActionState.trigger = ActionState::Trigger::onCast;
					cardPlayActionState.src = selectedId;
					cardPlayActionState.dst = card.type == SpellCard::Type::all ? -1 : target + (enemyResult != -1) * BOARD_CAPACITY_PER_SIDE;
					cardPlayActionState.dstUniqueId = cardPlayActionState.dst == -1 ? -1 : boardState.uniqueIds[cardPlayActionState.dst];
					cardPlayActionState.source = ActionState::Source::other;
					state.TryAddToStack(cardPlayActionState);
				}
			}

			selectionState = SelectionState::none;
		}

		if (activeStateValid || state.stack.count != 0)
			level->DrawFullCard(nullptr);
		return true;
	}

	bool MainLevel::CombatState::PreHandleActionState(State& state, const LevelUpdateInfo& info,
		ActionState& actionState)
	{
		info.screenShakeInfo.timeOut = 0;
		if (!ValidateActionState(state, activeState))
			return false;

		auto& boardState = state.boardState;
		if(actionState.trigger == ActionState::Trigger::onStartOfTurn)
		{
			const auto c = GetEventCardCount(state);
			for (uint32_t i = 0; i < c; ++i)
				previousEventCards[i] = eventCards[i];
			for (auto& eventCard : eventCards)
				eventCard = state.GetEvent(info, eventCards, c);
			++state.turn;

			// Untap.
			for (auto& b : state.tapped)
				b = false;

			if (state.maxMana < MAX_MANA)
				++state.maxMana;
			state.mana = state.maxMana;
		}
		else if(actionState.trigger == ActionState::Trigger::onAttack)
		{
			if (actionState.src < BOARD_CAPACITY_PER_SIDE)
			{
				auto& target = state.targets[actionState.dst - BOARD_CAPACITY_PER_SIDE];
				const auto oldTarget = target;
				if (boardState.allyCount > 1)
				{
					while (target == oldTarget)
						target = rand() % boardState.allyCount;
				}
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onSummon)
		{
			const bool isAlly = actionState.values[ActionState::VSummon::isAlly] == 1;
			const auto monsterId = actionState.values[ActionState::VSummon::id];
			const auto partyId = isAlly ? actionState.values[ActionState::VSummon::partyId] : -1;
			const auto attack = actionState.values[ActionState::VSummon::attack];
			const auto health = actionState.values[ActionState::VSummon::health];
			const auto untapped = actionState.values[ActionState::VSummon::untapped];

			const uint32_t targetId = isAlly ? boardState.allyCount : BOARD_CAPACITY_PER_SIDE + boardState.enemyCount;
			boardState.ids[targetId] = monsterId;
			boardState.combatStats[targetId] = GetCombatStat(info.monsters[monsterId]);
			boardState.uniqueIds[targetId] = uniqueId++;
			if(isAlly && targetId < PARTY_ACTIVE_CAPACITY)
				info.gameState.partyIds[targetId] = partyId;
			else if(!isAlly && boardState.allyCount > 0)
				state.targets[boardState.enemyCount] = rand() % boardState.allyCount;
			if (attack != -1)
				boardState.combatStats[targetId].attack = attack;
			if (health != -1)
				boardState.combatStats[targetId].health = health;
			if (isAlly)
				state.tapped[boardState.allyCount++] = untapped != 1;
			else
				++boardState.enemyCount;
			actionState.dst = targetId;
		}
		else if (actionState.trigger == ActionState::Trigger::onDraw)
			state.hand.Add() = state.Draw(info, state.hand.ptr, state.hand.count);

		return true;
	}

	void MainLevel::CombatState::CollectActivatedCards(State& state, const LevelUpdateInfo& info,
		const ActionState& actionState)
	{
		activations.Clear();

		const auto& boardState = state.boardState;
		const auto& gameState = info.gameState;

		const uint32_t allyCount = boardState.allyCount;
		for (uint32_t i = 0; i < allyCount; ++i)
		{
			bool activated = false;

			const auto flawId = info.gameState.curses[i];
			if (flawId != -1)
			{
				const auto curse = info.curses[flawId];
				if (curse.onActionEvent)
					if (curse.onActionEvent(info, state, actionState, i))
						activated = true;
			}

			for (uint32_t j = 0; j < gameState.artifactSlotCount; ++j)
			{
				const uint32_t artifactId = gameState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				if (artifactId == -1)
					continue;
				const auto& artifact = info.artifacts[artifactId];
				if (artifact.onActionEvent)
					if (artifact.onActionEvent(info, state, actionState, i))
						activated = true;
			}

			const auto id = boardState.ids[i];
			const auto& monster = info.monsters[id];
			if (monster.onActionEvent)
				if (monster.onActionEvent(info, state, actionState, i))
					activated = true;

			if (activated)
			{
				Activation activation{};
				activation.type = Activation::monster;
				activation.id = i;
				activations.Add() = activation;
			}
		}

		const auto& path = state.paths[state.GetPrimaryPath()];
		const auto& room = info.rooms[path.room];
		if (state.depth >= ROOMS_BEFORE_ROOM_EFFECTS && room.onActionEvent)
			if(room.onActionEvent(info, state, actionState, 0))
			{
				Activation activation{};
				activation.type = Activation::room;
				activations.Add() = activation;
			}

		const auto eventCount = GetEventCardCount(state);
		for (uint32_t i = 0; i < eventCount; ++i)
		{
			if (eventCards[i] == -1)
				continue;
			const auto& event = info.events[eventCards[i]];
			if (event.onActionEvent)
				if (event.onActionEvent(info, state, actionState, i))
				{
					Activation activation{};
					activation.type = Activation::event;
					activation.id = i;
					activations.Add() = activation;
				}
		}

		const uint32_t enemyCount = boardState.enemyCount;
		for (uint32_t i = 0; i < enemyCount; ++i)
		{
			const auto id = boardState.ids[BOARD_CAPACITY_PER_SIDE + i];
			const auto& monster = info.monsters[id];
			if (monster.onActionEvent)
				if (monster.onActionEvent(info, state, actionState, BOARD_CAPACITY_PER_SIDE + i))
				{
					Activation activation{};
					activation.id = BOARD_CAPACITY_PER_SIDE + i;
					activation.type = Activation::monster;
					activations.Add() = activation;
				}
		}
		for (uint32_t i = 0; i < state.hand.count; ++i)
		{
			const auto& spell = &info.spells[state.hand[i]];
			if (spell->onActionEvent)
				if (spell->onActionEvent(info, state, actionState, i))
				{
					Activation activation{};
					activation.id = i;
					activation.type = Activation::spell;
					activations.Add() = activation;
				}
		}
	}

	void MainLevel::CombatState::PostHandleActionState(State& state, const LevelUpdateInfo& info, 
		const Level* level, const ActionState& actionState)
	{
		auto& boardState = state.boardState;

		if (actionState.trigger == ActionState::Trigger::onStartOfTurn)
		{
			// Set new random enemy targets.
			for (uint32_t i = 0; i < boardState.enemyCount; ++i)
				state.targets[i] = boardState.allyCount == 0 ? -1 : rand() % boardState.allyCount;

			for (auto& combatStat : boardState.combatStats)
			{
				combatStat.tempAttack = 0;
				combatStat.tempHealth = 0;
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onAttack)
		{
			const auto& srcCombatStats = boardState.combatStats[actionState.src];

			constexpr auto vDamage = static_cast<uint32_t>(ActionState::VDamage::damage);
			ActionState damageState = actionState;
			damageState.trigger = ActionState::Trigger::onDamage;
			damageState.values[vDamage] = srcCombatStats.attack + srcCombatStats.tempAttack;
			state.TryAddToStack(damageState);
		}
		else if (actionState.trigger == ActionState::Trigger::onDamage)
		{
			auto& combatStats = boardState.combatStats[actionState.dst];
			auto& health = combatStats.health;
			auto& tempHealth = combatStats.tempHealth;

			if (health > 0)
			{
				constexpr auto vDamage = static_cast<uint32_t>(ActionState::VDamage::damage);
				auto damage = actionState.values[vDamage];

				const auto th = tempHealth;
				if (damage <= tempHealth)
				{
					tempHealth -= damage;
					damage = 0;
				}
				else
				{
					tempHealth = 0;
					damage -= th;
				}

				health = health < damage ? 0 : health - damage;
				metaDatas[META_DATA_ALLY_INDEX + actionState.dst].timeSinceStatsChanged = level->GetTime();

				if (health == 0)
				{
					ActionState deathActionState = actionState;
					deathActionState.trigger = ActionState::Trigger::onDeath;
					state.TryAddToStack(deathActionState);
				}
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onStatBuff)
		{
			auto& combatStats = boardState.combatStats[actionState.dst];
			constexpr auto vAtk = static_cast<uint32_t>(ActionState::VStatBuff::attack);
			constexpr auto vHlt = static_cast<uint32_t>(ActionState::VStatBuff::health);
			constexpr auto vTAtk = static_cast<uint32_t>(ActionState::VStatBuff::tempAttack);
			constexpr auto vTHlt = static_cast<uint32_t>(ActionState::VStatBuff::tempHealth);

			if(actionState.values[vAtk] != -1)
				combatStats.attack += actionState.values[vAtk];
			if (actionState.values[vHlt] != -1)
				combatStats.health += actionState.values[vHlt];
			if (actionState.values[vTAtk] != -1)
				combatStats.tempAttack += actionState.values[vTAtk];
			if (actionState.values[vTHlt] != -1)
				combatStats.tempHealth += actionState.values[vTHlt];

			metaDatas[META_DATA_ALLY_INDEX + actionState.dst].timeSinceStatsChanged = level->GetTime();
		}
		else if (actionState.trigger == ActionState::Trigger::onStatSet)
		{
			auto& combatStats = boardState.combatStats[actionState.dst];
			constexpr auto vAtk = static_cast<uint32_t>(ActionState::VStatSet::attack);
			constexpr auto vHlt = static_cast<uint32_t>(ActionState::VStatSet::health);
			constexpr auto vTAtk = static_cast<uint32_t>(ActionState::VStatSet::tempAttack);
			constexpr auto vTHlt = static_cast<uint32_t>(ActionState::VStatSet::tempHealth);

			if (actionState.values[vAtk] != -1)
				combatStats.attack = actionState.values[vAtk];
			if (actionState.values[vHlt] != -1)
				combatStats.health = actionState.values[vHlt];
			if (actionState.values[vTAtk] != -1)
				combatStats.tempAttack = actionState.values[vTAtk];
			if (actionState.values[vTHlt] != -1)
				combatStats.tempHealth = actionState.values[vTHlt];

			metaDatas[META_DATA_ALLY_INDEX + actionState.dst].timeSinceStatsChanged = level->GetTime();
		}
		else if (actionState.trigger == ActionState::Trigger::onDeath)
		{
			uint32_t i = actionState.dst;
			const bool isEnemy = i >= BOARD_CAPACITY_PER_SIDE;
			const uint32_t mod = BOARD_CAPACITY_PER_SIDE * isEnemy;
			i -= mod;
			const uint32_t c = (isEnemy ? boardState.enemyCount : boardState.allyCount) - 1;

			for (uint32_t j = actionState.dst + 2 + HAND_MAX_SIZE; j < BOARD_CAPACITY + HAND_MAX_SIZE + 1; ++j)
				metaDatas[j] = metaDatas[j + 1];

			if (isEnemy)
			{
				for (uint32_t j = i; j < c; ++j)
					state.targets[j] = state.targets[j + 1];
				const auto id = boardState.ids[i + mod];
				if(!info.monsters[id].unique)
					lastEnemyDefeatedId = id;
				--boardState.enemyCount;
			}
			else
			{
				auto& gameState = info.gameState;
				for (uint32_t j = i; j < c; ++j)
				{
					state.tapped[j] = state.tapped[j + 1];
					gameState.partyIds[j] = gameState.partyIds[j + 1];
					memcpy(&gameState.artifacts[j * MONSTER_ARTIFACT_CAPACITY], &gameState.artifacts[(j + 1) * MONSTER_ARTIFACT_CAPACITY], 
						sizeof(uint32_t) * MONSTER_ARTIFACT_CAPACITY);
					gameState.curses[j] = gameState.curses[j + 1];
				}
				for (uint32_t k = 0; k < MONSTER_ARTIFACT_CAPACITY; ++k)
					gameState.artifacts[c * MONSTER_ARTIFACT_CAPACITY + k] = -1;
				gameState.curses[c] = -1;
				--boardState.allyCount;
			}

			// Remove shared data.
			for (uint32_t j = i; j < c; ++j)
			{
				boardState.ids[j + mod] = boardState.ids[j + 1 + mod];
				boardState.uniqueIds[j + mod] = boardState.uniqueIds[j + 1 + mod];
				boardState.combatStats[j + mod] = boardState.combatStats[j + 1 + mod];
				metaDatas[META_DATA_ALLY_INDEX + j + mod] = metaDatas[META_DATA_ALLY_INDEX + j + mod + 1];
			}

			// Modify states as to ensure other events can go through.
			for (auto& as : state.stack)
			{
				if (as.source == ActionState::Source::board)
				{
					if(isEnemy && as.src > i + BOARD_CAPACITY_PER_SIDE)
						--as.src;
					else if(!isEnemy && as.src > i && as.src < BOARD_CAPACITY_PER_SIDE)
						--as.src;
				}

				if (isEnemy && as.dst > i + BOARD_CAPACITY_PER_SIDE)
					--as.dst;
				else if (!isEnemy && as.dst > i && as.dst < BOARD_CAPACITY_PER_SIDE)
					--as.dst;
			}
		}
		else if (actionState.trigger == ActionState::Trigger::onCast)
		{
			state.hand.RemoveAtOrdered(actionState.src);
			for (uint32_t i = 0; i < HAND_MAX_SIZE - 1; ++i)
				metaDatas[META_DATA_HAND_INDEX + i] = metaDatas[META_DATA_HAND_INDEX + i + 1];
		}
	}

	bool MainLevel::CombatState::ValidateActionState(const State& state, const ActionState& actionState)
	{
		const auto& boardState = state.boardState;

		const bool handSrc = actionState.source == ActionState::Source::other;
		bool validActionState;
		bool validSrc;
		bool validDst;

		switch (actionState.trigger)
		{
		case ActionState::Trigger::onDamage:
		case ActionState::Trigger::onAttack:
		case ActionState::Trigger::onDeath:
		case ActionState::Trigger::onStatBuff:
		case ActionState::Trigger::onStatSet:
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
		case ActionState::Trigger::onDraw:
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
		if (!activeStateValid)
			return;
		if (activeState.trigger != ActionState::Trigger::onAttack)
			return;
		
		assert(activeState.trigger == ActionState::Trigger::onAttack);

		uint32_t src = activeState.src;
		uint32_t dst = activeState.dst;

		if (src >= BOARD_CAPACITY_PER_SIDE)
			src -= BOARD_CAPACITY_PER_SIDE;
		else if (dst >= BOARD_CAPACITY_PER_SIDE)
			dst -= BOARD_CAPACITY_PER_SIDE;
		
		const bool isSrc = allied ? activeState.src < BOARD_CAPACITY_PER_SIDE : activeState.src >= BOARD_CAPACITY_PER_SIDE;

		// Move towards each other.
		const float delta = GetAttackMoveOffset(state, activeState);

		const float moveDuration = GetAttackMoveDuration(state, activeState);
		const float current = level.GetTime() - timeSinceLastActionState;

		const auto curve = je::CreateCurveOvershooting();
		const auto curveDown = je::CreateCurveDecelerate();

		float eval = 0;
		if (moveDuration > 1e-5f)
		{
			if(current <= moveDuration + CARD_VERTICAL_MOVE_SPEED * 2)
			{
				const float lerp = current / moveDuration;
				eval = curve.Evaluate(lerp);
			}
			else
			{
				const float lerp = (current - (moveDuration + CARD_VERTICAL_MOVE_SPEED * 2)) / moveDuration;
				eval = curveDown.Evaluate(1.f - lerp);
			}
		}
		
		const auto cardShape = GetCardShape(info, drawInfo);
		drawInfo.centerOffset += eval * delta * cardShape.x * (2 * !allied - 1);

		// Move towards dst vertically.
		if(isSrc && current >= moveDuration)
		{
			const float l = GetActionStateLerp(level, CARD_VERTICAL_MOVE_SPEED * 2, moveDuration);
			const float vEval = DoubleCurveEvaluate(l, curve, curveDown);
			drawInfo.overridePosIndex = src;
			drawInfo.overridePos = GetCardPosition(info, drawInfo, src);
			drawInfo.overridePos.y += vEval * (ENEMY_HEIGHT - ALLY_HEIGHT - cardShape.y) * (2 * allied - 1);
		}
		// On hit.
		else if(!isSrc && current >= moveDuration + CARD_VERTICAL_MOVE_SPEED)
		{
			const float l = GetActionStateLerp(level, CARD_VERTICAL_MOVE_SPEED, moveDuration + CARD_VERTICAL_MOVE_SPEED);
			const float vEval = DoubleCurveEvaluate(l, curveDown, curve);
			drawInfo.overridePosIndex = dst;
			drawInfo.overridePos = GetCardPosition(info, drawInfo, dst);
			drawInfo.overridePos.y += vEval * cardShape.y * .5f * (2 * !allied - 1);
			Shake(info);
		}
	}

	void MainLevel::CombatState::DrawDamageAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeStateValid)
			return;
		if (allied && activeState.dst >= BOARD_CAPACITY_PER_SIDE || !allied && activeState.dst < BOARD_CAPACITY_PER_SIDE)
			return;

		if (activeState.trigger != ActionState::Trigger::onDamage)
			return;

		const auto pos = GetCardPosition(info, drawInfo, activeState.dst - !allied * BOARD_CAPACITY_PER_SIDE);

		TextTask textTask{};
		textTask.center = true;
		textTask.position = pos;
		textTask.priority = true;

		const uint32_t dmg = activeState.values[static_cast<uint32_t>(ActionState::VDamage::damage)];
		textTask.text = TextInterpreter::IntToConstCharPtr(dmg, info.frameArena);
		textTask.color = glm::vec4(1, 0, 0, 1);
		textTask.text = TextInterpreter::Concat("-", textTask.text, info.frameArena);

		drawInfo.damagedIndex = activeState.dst - !allied * BOARD_CAPACITY_PER_SIDE;
		
		const auto curve = je::CreateCurveOvershooting();
		const float eval = curve.Evaluate(GetActionStateLerp(level));

		textTask.position.y += eval * 24;
		info.textTasks.Push(textTask);
		Shake(info);
	}

	void MainLevel::CombatState::DrawBuffAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, bool allied) const
	{
		if (!activeStateValid)
			return;
		if (allied && activeState.dst >= BOARD_CAPACITY_PER_SIDE || !allied && activeState.dst < BOARD_CAPACITY_PER_SIDE)
			return;

		if (activeState.trigger != ActionState::Trigger::onStatBuff)
			return;

		const auto pos = GetCardPosition(info, drawInfo, activeState.dst - !allied * BOARD_CAPACITY_PER_SIDE);

		TextTask textTask{};
		textTask.center = true;
		textTask.position = pos;
		textTask.priority = true;

		uint32_t atkBuff = activeState.values[static_cast<uint32_t>(ActionState::VStatBuff::attack)];
		if (atkBuff == -1)
			atkBuff = 0;

		textTask.text = TextInterpreter::IntToConstCharPtr(atkBuff, info.frameArena);
		textTask.color = glm::vec4(0, 1, 0, 1);
		textTask.text = TextInterpreter::Concat("+", textTask.text, info.frameArena);
		textTask.text = TextInterpreter::Concat(textTask.text, "/+", info.frameArena);

		uint32_t hltBuff = activeState.values[static_cast<uint32_t>(ActionState::VStatBuff::health)];
		if (hltBuff == -1)
			hltBuff = 0;

		const char* t = TextInterpreter::IntToConstCharPtr(hltBuff, info.frameArena);
		textTask.text = TextInterpreter::Concat(textTask.text, t, info.frameArena);
		
		const auto curve = je::CreateCurveOvershooting();
		const float eval = curve.Evaluate(GetActionStateLerp(level));

		textTask.position.y += eval * 24;

		if(atkBuff != 0 || hltBuff != 0)
			info.textTasks.Push(textTask);
	}

	void MainLevel::CombatState::DrawSummonAnimation(const LevelUpdateInfo& info, const Level& level,
		CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeStateValid)
			return;
		if (activeState.trigger != ActionState::Trigger::onSummon)
			return;
		if (allied != activeState.values[static_cast<uint32_t>(ActionState::VSummon::isAlly)])
			return;

		drawInfo.spawning = true;
		drawInfo.spawnLerp = GetActionStateLerp(level);
	}

	void MainLevel::CombatState::DrawDrawAnimation(const Level& level, CardSelectionDrawInfo& drawInfo) const
	{
		if (!activeStateValid)
			return;
		
		drawInfo.spawning = true;
		drawInfo.spawnLerp = GetActionStateLerp(level, CARD_DRAW_DURATION);
	}

	void MainLevel::CombatState::DrawDeathAnimation(const Level& level, CardSelectionDrawInfo& drawInfo, const bool allied) const
	{
		if (!activeStateValid)
			return;
		if (activeState.trigger != ActionState::Trigger::onDeath)
			return;
		if (allied && activeState.dst >= BOARD_CAPACITY_PER_SIDE || !allied && activeState.dst < BOARD_CAPACITY_PER_SIDE)
			return;
		
		drawInfo.fadeIndex = activeState.dst;
		if (activeState.dst >= BOARD_CAPACITY_PER_SIDE)
			drawInfo.fadeIndex -= BOARD_CAPACITY_PER_SIDE;
		drawInfo.fadeLerp = GetActionStateLerp(level);
	}

	void MainLevel::CombatState::DrawActivationAnimation(CardSelectionDrawInfo& drawInfo, 
		const Activation::Type type, const uint32_t idMod) const
	{
		if (!activeStateValid)
			return;
		if (activations.count == 0)
			return;
		const auto& activation = activations.Peek();
		if (activation.type != type)
			return;
		if (activation.id - idMod >= drawInfo.length)
			return;
		drawInfo.activationIndex = activation.id - idMod;
		const float f = (CARD_ACTIVATION_DURATION - (CARD_ACTIVATION_DURATION - activationDuration)) / CARD_ACTIVATION_DURATION;
		drawInfo.activationLerp = f;
	}

	void MainLevel::CombatState::DrawCardPlayAnimation(const Level& level, CardSelectionDrawInfo& drawInfo) const
	{
		if (!activeStateValid)
			return;
		if (activeState.trigger != ActionState::Trigger::onCast)
			return;
		DrawFadeAnimation(level, drawInfo, activeState.src);
	}

	void MainLevel::CombatState::DrawFadeAnimation(const Level& level, CardSelectionDrawInfo& drawInfo, const uint32_t src) const
	{
		if (!activeStateValid)
			return;
		drawInfo.fadeIndex = src;
		drawInfo.fadeLerp = GetActionStateLerp(level);
	}

	float MainLevel::CombatState::GetActionStateLerp(const Level& level, const float duration, const float startoffset) const
	{
		const float t = level.GetTime();
		const float aTime = t - (timeSinceLastActionState + startoffset);
		const float a = (duration - (duration - aTime)) / duration;
		return jv::Clamp(a, 0.f, 1.f);
	}

	float MainLevel::CombatState::GetAttackMoveOffset(const State& state, const ActionState& actionState) const
	{
		const auto& boardState = state.boardState;

		uint32_t src = actionState.src;
		uint32_t dst = actionState.dst;

		if (src >= BOARD_CAPACITY_PER_SIDE)
			src -= BOARD_CAPACITY_PER_SIDE;
		else if (dst >= BOARD_CAPACITY_PER_SIDE)
			dst -= BOARD_CAPACITY_PER_SIDE;

		const bool allied = src <= BOARD_CAPACITY_PER_SIDE;
		const uint32_t c = allied ? boardState.allyCount : boardState.enemyCount;
		const uint32_t oC = allied ? boardState.enemyCount : boardState.allyCount;
		const bool isSrc = allied ? activeState.src < BOARD_CAPACITY_PER_SIDE : activeState.src >= BOARD_CAPACITY_PER_SIDE;

		const float ind = static_cast<float>(isSrc ? src : dst);
		const float offset = ind - static_cast<float>(c - 1) / 2;

		const float oInd = static_cast<float>(isSrc ? dst : src);
		const float oOffset = oInd - static_cast<float>(oC - 1) / 2;

		// Move towards each other.
		const float delta = (offset - oOffset) / 2;
		return delta;
	}

	float MainLevel::CombatState::GetAttackMoveDuration(const State& state, const ActionState& actionState) const
	{
		return fabs(GetAttackMoveOffset(state, actionState)) / CARD_HORIZONTAL_MOVE_SPEED;
	}

	uint32_t MainLevel::CombatState::GetEventCardCount(const State& state)
	{
		if (state.depth < ROOMS_BEFORE_EVENT_EFFECTS)
			return 0;
		return jv::Min(1 + (state.depth - ROOMS_BEFORE_EVENT_EFFECTS) / ROOM_COUNT_BEFORE_BOSS, EVENT_CARD_MAX_COUNT);
	}

	void MainLevel::CombatState::Shake(const LevelUpdateInfo& info)
	{
		if (!info.screenShakeInfo.IsInTimeOut())
		{
			info.screenShakeInfo.fallOfThreshold = .1f;
			info.screenShakeInfo.intensity = 2;
			info.screenShakeInfo.remaining = .1f;
			info.screenShakeInfo.timeOut = 1e4;
		}
	}

	void MainLevel::CombatState::OnExit(State& state, const LevelInfo& info)
	{
		LevelState<State>::OnExit(state, info);
		++state.depth;
	}

	void MainLevel::CombatState::DrawParallaxBackground(const LevelUpdateInfo& info, const bool mirror)
	{
		glm::vec2 mouseOffset = info.inputState.mousePos - SIMULATED_RESOLUTION / 2;

		PixelPerfectRenderTask renderTask{};
		renderTask.scale = glm::ivec2(64);
		renderTask.xCenter = true;

		auto subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::flowers)].subTexture;
		jv::ge::SubTexture subTextures[5];
		Divide(subTexture, subTextures, sizeof subTextures / sizeof(jv::ge::SubTexture));

		glm::ivec2 c = glm::ivec2(0, 24);
		c.x = mirror ? SIMULATED_RESOLUTION.x - c.x : c.x;
		const int32_t m = !mirror * 2 - 1;

		renderTask.subTexture = subTextures[4];
		if (mirror)
			renderTask.subTexture = Mirror(renderTask.subTexture);
		renderTask.position = c + glm::ivec2(128 * m, -16);
		renderTask.position -= mouseOffset * glm::vec2(.002f);
		info.renderTasks.Push(renderTask);

		renderTask.subTexture = subTextures[3];
		if (mirror)
			renderTask.subTexture = Mirror(renderTask.subTexture);
		renderTask.position = c + glm::ivec2(76 * m, 8);
		renderTask.position -= mouseOffset * glm::vec2(.005f);
		info.renderTasks.Push(renderTask);

		renderTask.subTexture = subTextures[2];
		if (mirror)
			renderTask.subTexture = Mirror(renderTask.subTexture);
		renderTask.position = c + glm::ivec2(48 * m, 8);
		renderTask.position -= mouseOffset * glm::vec2(.01f);
		info.renderTasks.Push(renderTask);

		renderTask.subTexture = subTextures[1];
		if (mirror)
			renderTask.subTexture = Mirror(renderTask.subTexture);
		renderTask.position = c + glm::ivec2(36 * m, 8);
		renderTask.position -= mouseOffset * glm::vec2(.02f);
		info.renderTasks.Push(renderTask);
		
		renderTask.subTexture = subTextures[0];
		if (mirror)
			renderTask.subTexture = Mirror(renderTask.subTexture);
		renderTask.position = c;
		renderTask.position -= mouseOffset * glm::vec2(.04f);
		info.renderTasks.Push(renderTask);
	}

	void MainLevel::RewardMagicCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
		for (auto& metaData : metaDatas)
			metaData = {};
	}

	bool MainLevel::RewardMagicCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		Card* cards[SPELL_DECK_SIZE];
		uint32_t costs[SPELL_DECK_SIZE];
		for (uint32_t i = 0; i < SPELL_DECK_SIZE; ++i)
		{
			const auto c = &info.spells[info.gameState.spells[i]];
			cards[i] = c;
			costs[i] = c->cost;
		}

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2;// +cardTexture.resolution.y / 2;
		cardSelectionDrawInfo.highlighted = discoverOption;
		cardSelectionDrawInfo.length = SPELL_DECK_SIZE;
		cardSelectionDrawInfo.costs = costs;
		cardSelectionDrawInfo.metaDatas = metaDatas;
		cardSelectionDrawInfo.lifeTime = level->GetTime();
		cardSelectionDrawInfo.rowCutoff = 6;
		const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		const auto& path = state.paths[state.chosenPath];

		const auto rewardCard = &info.spells[path.spell];
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = rewardCard;
		cardDrawInfo.center = true;
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y / 2 + 44 };
		cardDrawInfo.lifeTime = level->GetTime();
		cardDrawInfo.cost = rewardCard->cost;
		cardDrawInfo.metaData = &metaDatas[SPELL_DECK_SIZE];
		level->DrawCard(info, cardDrawInfo);

		if (info.inputState.lMouse.PressEvent())
			discoverOption = choice == discoverOption ? -1 : choice;

		const char* text = "select a card to replace, if any.";
		level->DrawTopCenterHeader(info, HeaderSpacing::far, text);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::far, f);
		
		if (!level->GetIsLoading() && info.inputState.enter.PressEvent())
		{
			if (discoverOption != -1)
				info.gameState.spells[discoverOption] = path.spell;
			stateIndex = static_cast<uint32_t>(StateNames::pathSelect);
		}

		return true;
	}

	void MainLevel::RewardFlawCardState::Reset(State& state, const LevelInfo& info)
	{
		discoverOption = -1;
		for (auto& metaData : metaDatas)
			metaData = {};
	}

	bool MainLevel::RewardFlawCardState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const auto& gameState = info.gameState;

		bool greyedOut[PARTY_ACTIVE_CAPACITY];
		bool flawSlotAvailable = false;

		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const auto& curse = info.gameState.curses[i];
			greyedOut[i] = curse != -1;
			flawSlotAvailable = flawSlotAvailable ? true : curse == -1;
		}

		if (flawSlotAvailable)
		{
			level->DrawTopCenterHeader(info, HeaderSpacing::normal, "select the bearer of this curse.");

			Card* cards[PARTY_ACTIVE_CAPACITY];
			CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const auto monster = &info.monsters[gameState.monsterIds[i]];
				cards[i] = monster;
				combatStats[i] = GetCombatStat(*monster);
				combatStats[i].health = gameState.healths[i];
			}

			Card** stacks[PARTY_CAPACITY]{};
			uint32_t stackCounts[PARTY_CAPACITY]{};
			
			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const uint32_t count = stackCounts[i] = gameState.artifactSlotCount;
				if (count == 0)
					continue;

				const auto arr = jv::CreateArray<Card*>(info.frameArena, count + 1);
				stacks[i] = arr.ptr;
				for (uint32_t j = 0; j < count; ++j)
				{
					const uint32_t index = gameState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
					arr[j] = index == -1 ? nullptr : &info.artifacts[index];
				}

				const uint32_t curse = info.gameState.curses[i];
				if (curse != -1)
				{
					++stackCounts[i];
					stacks[i][count] = &info.curses[curse];
				}
			}

			CardSelectionDrawInfo cardSelectionDrawInfo{};
			cardSelectionDrawInfo.cards = cards;
			cardSelectionDrawInfo.length = gameState.partySize;
			cardSelectionDrawInfo.greyedOutArr = greyedOut;
			cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
			cardSelectionDrawInfo.stacks = stacks;
			cardSelectionDrawInfo.stackCounts = stackCounts;
			cardSelectionDrawInfo.highlighted = discoverOption;
			cardSelectionDrawInfo.combatStats = combatStats;
			cardSelectionDrawInfo.metaDatas = metaDatas;
			cardSelectionDrawInfo.lifeTime = level->GetTime();
			const uint32_t choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
			
			const auto& path = state.paths[state.chosenPath];

			CardDrawInfo cardDrawInfo{};
			cardDrawInfo.card = &info.curses[path.curse];
			cardDrawInfo.center = true;
			cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
			cardDrawInfo.lifeTime = level->GetTime();
			cardDrawInfo.metaData = &metaDatas[PARTY_ACTIVE_CAPACITY];
			level->DrawCard(info, cardDrawInfo);

			if (!level->GetIsLoading() && info.inputState.lMouse.PressEvent())
			{
				discoverOption = choice == discoverOption ? -1 : choice;
				if (discoverOption != -1)
					timeSinceDiscovered = level->GetTime();
			}

			if (discoverOption == -1)
				return true;
			
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, level->GetTime() - timeSinceDiscovered);
			if (level->GetIsLoading() || !info.inputState.enter.PressEvent())
				return true;

			info.gameState.curses[discoverOption] = path.curse;
		}

		stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);
		return true;
	}

	void MainLevel::RewardArtifactState::Reset(State& state, const LevelInfo& info)
	{
		for (auto& metaData : metaDatas)
			metaData = {};
		if (state.depth % ROOM_COUNT_BEFORE_BOSS == 0)
		{
			auto& gameState = info.gameState;

			for (uint32_t i = 0; i < gameState.partySize; ++i)
			{
				const uint32_t id = gameState.partyIds[i];
				auto& slotCount = gameState.artifactSlotCount;
				slotCount = jv::Max(slotCount, 1 + state.depth / (ROOM_COUNT_BEFORE_BOSS * 2));
				slotCount = jv::Min(slotCount, MONSTER_ARTIFACT_CAPACITY);
			}
		}
	}

	bool MainLevel::RewardArtifactState::Update(State& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		Card* cards[PARTY_ACTIVE_CAPACITY]{};
		
		auto& gameState = info.gameState;

		const char* text = "you may swap artifacts around.";
		level->DrawTopCenterHeader(info, HeaderSpacing::normal, text, 1);
		const float f = level->GetTime() - static_cast<float>(strlen(text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			level->DrawPressEnterToContinue(info, HeaderSpacing::normal, f);

		for (uint32_t i = 0; i < gameState.partySize; ++i)
			cards[i] = &info.monsters[gameState.monsterIds[i]];

		Card** artifacts[PARTY_ACTIVE_CAPACITY]{};
		CombatStats combatStats[PARTY_ACTIVE_CAPACITY];
		uint32_t artifactCounts[PARTY_ACTIVE_CAPACITY]{};
		
		for (uint32_t i = 0; i < gameState.partySize; ++i)
		{
			const auto monster = &info.monsters[gameState.monsterIds[i]];
			cards[i] = monster;
			combatStats[i] = GetCombatStat(*monster);
			combatStats[i].health = gameState.healths[i];

			const uint32_t count = artifactCounts[i] = gameState.artifactSlotCount;
			const auto arr = artifacts[i] = jv::CreateArray<Card*>(info.frameArena, MONSTER_ARTIFACT_CAPACITY).ptr;
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = gameState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		uint32_t outStackSelected;
		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = gameState.partySize;
		cardSelectionDrawInfo.height = SIMULATED_RESOLUTION.y / 2 - cardTexture.resolution.y - 2;
		cardSelectionDrawInfo.stacks = artifacts;
		cardSelectionDrawInfo.stackCounts = artifactCounts;
		cardSelectionDrawInfo.outStackSelected = &outStackSelected;
		cardSelectionDrawInfo.combatStats = combatStats;
		cardSelectionDrawInfo.metaDatas = metaDatas;
		cardSelectionDrawInfo.lifeTime = level->GetTime();
		const auto choice = level->DrawCardSelection(info, cardSelectionDrawInfo);
		
		auto& path = state.paths[state.chosenPath];
		
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.card = path.artifact == -1 ? nullptr : &info.artifacts[path.artifact];
		cardDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 + cardTexture.resolution.y + 2 };
		cardDrawInfo.center = true;
		cardDrawInfo.lifeTime = level->GetTime();
		cardDrawInfo.metaData = &metaDatas[PARTY_ACTIVE_CAPACITY];
		level->DrawCard(info, cardDrawInfo);
		
		if(choice != -1 && outStackSelected != -1)
		{
			if (!level->GetIsLoading() && info.inputState.lMouse.PressEvent())
			{
				const uint32_t swappable = path.artifact;
				path.artifact = gameState.artifacts[choice * MONSTER_ARTIFACT_CAPACITY + outStackSelected];
				gameState.artifacts[choice * MONSTER_ARTIFACT_CAPACITY + outStackSelected] = swappable;
			}
		}

		if (!level->GetIsLoading() && info.inputState.enter.PressEvent())
			stateIndex = static_cast<uint32_t>(StateNames::rewardMagic);

		return true;
	}

	void MainLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		
		const auto states = jv::CreateArray<LevelState<State>*>(info.arena, 6);
		states[0] = info.arena.New<BossRevealState>();
		states[1] = info.arena.New<PathSelectState>();
		states[2] = info.arena.New<CombatState>();
		states[3] = info.arena.New<RewardMagicCardState>();
		states[4] = info.arena.New<RewardFlawCardState>();
		states[5] = info.arena.New<RewardArtifactState>();
		stateMachine = LevelStateMachine<State>::Create(info, states, State::Create(info));
		ingameMenuOpened = false;
	}

	bool MainLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		if (info.inputState.esc.PressEvent())
		{
			ingameMenuOpened = !ingameMenuOpened;
			timeSinceIngameMenuOpened = GetTime();
		}

		if (ingameMenuOpened)
		{
			info.inCombat = false;
			const float lifetime = GetTime() - timeSinceIngameMenuOpened;

			DrawTopCenterHeader(info, HeaderSpacing::close, "pause menu", 2, lifetime);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.center = true;
			buttonDrawInfo.origin = SIMULATED_RESOLUTION / 2 + glm::ivec2(0, 10);
			
			buttonDrawInfo.text = "resume";
			if(DrawButton(info, buttonDrawInfo, lifetime))
			{
				ingameMenuOpened = false;
				return true;
			}

			buttonDrawInfo.text = "main menu";
			buttonDrawInfo.origin.y -= 20;
			if (DrawButton(info, buttonDrawInfo, lifetime))
			{
				loadLevelIndex = LevelIndex::mainMenu;
				return true;
			}

			buttonDrawInfo.text = "exit game";
			buttonDrawInfo.origin.y -= 20;
			if (DrawButton(info, buttonDrawInfo, lifetime))
			{
				return false;
			}

			return true;
		}

		return stateMachine.Update(info, this, loadLevelIndex);
	}
}
