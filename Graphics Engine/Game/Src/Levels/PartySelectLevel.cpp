#include "pch_game.h"
#include "Levels/PartySelectLevel.h"

#include "Levels/LevelUtils.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"

namespace game
{
	void PartySelectLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		info.gameState = GameState::Create();
		for (auto& b : selected)
			b = false;
		lastHovered = -1;
	}

	bool PartySelectLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		const auto& playerState = info.playerState;

		Card* cards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			cards[i] = &info.monsters[playerState.monsterIds[i]];

		const auto tempScope = info.tempArena.CreateScope();

		ArtifactCard** artifacts[PARTY_CAPACITY]{};
		uint32_t artifactCounts[PARTY_CAPACITY]{};

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

		RenderMonsterCardInfo monsterRenderInfo{};
		monsterRenderInfo.levelUpdateInfo = &info;
		monsterRenderInfo.cards = cards;
		monsterRenderInfo.length = playerState.partySize;
		monsterRenderInfo.selectedArr = selected;
		monsterRenderInfo.artifactArr = artifacts;
		monsterRenderInfo.artifactCounts = artifactCounts;
		const uint32_t choice = RenderMonsterCards(info.frameArena, monsterRenderInfo).selectedMonster;

		info.tempArena.DestroyScope(tempScope);

		if (choice != -1)
			lastHovered = choice;

		if (info.inputState.lMouse == InputState::pressed && choice != -1)
			selected[choice] = !selected[choice];

		TextTask textTask{};
		textTask.center = true;
		textTask.text = "select up to 4 party members.";
		textTask.position = TEXT_CENTER_TOP_POSITION;
		textTask.scale = TEXT_BIG_SCALE;
		info.textTasks.Push(textTask);

		uint32_t selectedAmount = 0;
		for (const auto& b : selected)
			selectedAmount += 1 * b;

		if(selectedAmount > 0 && selectedAmount <= PARTY_ACTIVE_CAPACITY)
		{
			textTask.position = TEXT_CENTER_BOT_POSITION;
			textTask.text = "press enter to continue.";
			info.textTasks.Push(textTask);

			auto& gameState = info.gameState;
			gameState.partySize = selectedAmount;

			uint32_t j = 0;
			for (uint32_t i = 0; i < PARTY_CAPACITY; ++i)
			{
				if (!selected[i])
					continue;
				gameState.partyIds[j] = i;

				const auto& monster = info.monsters[playerState.monsterIds[i]];
				gameState.healths[j++] = monster.health;
			}

			if (info.inputState.enter == InputState::pressed)
				loadLevelIndex = LevelIndex::main;
		}

		return true;
	}
}
