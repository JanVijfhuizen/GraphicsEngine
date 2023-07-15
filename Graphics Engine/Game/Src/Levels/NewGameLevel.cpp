#include "pch_game.h"
#include "Levels/NewGameLevel.h"
#include "CardGame.h"
#include "Levels/LevelUtils.h"
#include "LevelStates/LevelStateMachine.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
#include "Utils/Shuffle.h"

namespace game
{
	void NewGameLevel::Create(const LevelCreateInfo& info)
	{
		ClearSaveData();

		const auto states = jv::CreateArray<LevelState<StateInfo>*>(info.arena, 3);
		states[0] = info.arena.New<ModeSelectState>();
		states[1] = info.arena.New<PartySelectState>();
		states[2] = info.arena.New<JoinState>();
		stateMachine = LevelStateMachine<StateInfo>::Create(info, states);
	}

	bool NewGameLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		return stateMachine.Update(info, loadLevelIndex);
	}

	bool NewGameLevel::ModeSelectState::Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex)
	{
		TextTask textTask{};
		textTask.center = true;
		textTask.text = "choose a mode.";
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
				stateIndex = 1;
				return true;
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
				stateIndex = 1;
				return true;
			}

		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "iron man";
		info.textTasks.Push(buttonTextTask);
		return true;
	}

	bool NewGameLevel::PartySelectState::Create(StateInfo& state, const LevelCreateInfo& info)
	{
		uint32_t count;
		GetDeck(nullptr, &count, info.monsters);
		monsterDeck = jv::CreateVector<uint32_t>(info.arena, count);
		GetDeck(nullptr, &count, info.artifacts);
		artifactDeck = jv::CreateVector<uint32_t>(info.arena, count);

		GetDeck(&monsterDeck, nullptr, info.monsters);
		GetDeck(&artifactDeck, nullptr, info.artifacts);

		// Create a discover option for your initial monster.
		monsterDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(monsterDeck.ptr, monsterDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			monsterDiscoverOptions[i] = monsterDeck.Pop();
		// Create a discover option for your initial artifact.
		artifactDiscoverOptions = jv::CreateArray<uint32_t>(info.arena, DISCOVER_LENGTH);
		Shuffle(artifactDeck.ptr, artifactDeck.count);
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
			artifactDiscoverOptions[i] = artifactDeck.Pop();
		return true;
	}

	bool NewGameLevel::PartySelectState::Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex)
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
		auto choice = RenderMonsterCards(info.frameArena, renderInfo);
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
			{
				state.monsterId = monsterChoice;
				state.artifactId = artifactChoice;
				stateIndex = 2;
			}
		}
		return true;
	}

	bool NewGameLevel::JoinState::Update(StateInfo& state, const LevelUpdateInfo& info, uint32_t& stateIndex,
		LevelIndex& loadLevelIndex)
	{
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
			playerState.monsterIds[0] = state.monsterId;
			for (auto& artifact : playerState.artifacts)
				artifact = -1;
			playerState.artifactSlotCounts[0] = 1;
			playerState.artifacts[0] = state.artifactId;
			playerState.monsterIds[1] = 0;
			playerState.partySize = 2;
			SaveData(playerState);
			loadLevelIndex = LevelIndex::partySelect;
		}
		return true;
	}
}
