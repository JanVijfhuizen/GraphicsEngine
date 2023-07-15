#include "pch_game.h"
#include "Levels/PartySelectLevel.h"

#include "Levels/LevelUtils.h"
#include "States/GameState.h"
#include "States/InputState.h"

namespace game
{
	void PartySelectLevel::Create(const LevelCreateInfo& info)
	{
		info.gameState = {};
		for (auto& b : selected)
			b = false;
	}

	bool PartySelectLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		const auto& playerState = info.playerState;

		Card* cards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			cards[i] = &info.monsters[playerState.monsterIds[i]];

		RenderCardInfo monsterRenderInfo{};
		monsterRenderInfo.levelUpdateInfo = &info;
		monsterRenderInfo.cards = cards;
		monsterRenderInfo.length = playerState.partySize;
		monsterRenderInfo.selectedArr = selected;
		const uint32_t choice = RenderMonsterCards(info.frameArena, monsterRenderInfo);

		if(choice != -1)
		{
			const uint32_t artifactSlotCount = playerState.artifactSlotCounts[choice];
			for (uint32_t i = 0; i < artifactSlotCount; ++i)
			{
				const uint32_t index = playerState.artifacts[choice * MONSTER_ARTIFACT_CAPACITY + i];
				cards[i] = index == -1 ? nullptr : &info.artifacts[index];
			}

			RenderCardInfo artifactRenderInfo{};
			artifactRenderInfo.levelUpdateInfo = &info;
			artifactRenderInfo.cards = cards;
			artifactRenderInfo.length = artifactSlotCount;
			artifactRenderInfo.center.y += CARD_HEIGHT * 2;
			RenderCards(artifactRenderInfo);
		}

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

		if(selectedAmount > 0 && selectedAmount < PARTY_ACTIVE_CAPACITY)
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
				gameState.partyMembers[j++] = i;
			}

			if (info.inputState.enter == InputState::pressed)
				loadLevelIndex = LevelIndex::main;
		}

		return true;
	}
}
