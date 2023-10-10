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
		timeSincePartySelected = -1;
		Level::Create(info);
		info.gameState = GameState::Create();
		for (auto& b : selected)
			b = false;
	}

	bool PartySelectLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		PartyDrawInfo partyDrawInfo{};
		partyDrawInfo.height = SIMULATED_RESOLUTION.y / 2;
		partyDrawInfo.selectedArr = selected;
		const uint32_t choice = DrawParty(info, partyDrawInfo);
		
		if (info.inputState.lMouse.PressEvent() && choice != -1)
			selected[choice] = !selected[choice];

		DrawTopCenterHeader(info, HeaderSpacing::close, "select up to 4 party members.");
		
		uint32_t selectedAmount = 0;
		for (const auto& b : selected)
			selectedAmount += 1 * b;

		if (!GetIsLoading() && selectedAmount > 0 && selectedAmount <= PARTY_ACTIVE_INITIAL_CAPACITY)
		{
			if (timeSincePartySelected < 0)
				timeSincePartySelected = GetTime();

			DrawPressEnterToContinue(info, HeaderSpacing::close, GetTime() - timeSincePartySelected);

			if (info.inputState.enter.PressEvent())
			{
				auto& gameState = info.gameState;
				gameState.partyCount = selectedAmount;

				const auto& playerState = info.playerState;

				uint32_t j = 0;
				for (uint32_t i = 0; i < PARTY_ACTIVE_INITIAL_CAPACITY; ++i)
				{
					if (!selected[i])
						continue;
					gameState.partyIds[j] = i;
					gameState.monsterIds[j] = info.playerState.monsterIds[i];
					memcpy(&gameState.artifacts[j * MONSTER_ARTIFACT_CAPACITY], &playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY], 
						sizeof(uint32_t) * MONSTER_ARTIFACT_CAPACITY);
					gameState.artifactSlotCounts[j] = playerState.artifactSlotCounts[i];

					const auto& monster = info.monsters[info.playerState.monsterIds[i]];
					gameState.healths[j++] = monster.health;
				}
				
				Load(LevelIndex::main, true);
			}
		}
		else
			timeSincePartySelected = -1;

		return true;
	}
}
