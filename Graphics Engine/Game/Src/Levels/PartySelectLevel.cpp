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

		if (!GetIsLoading() && selectedAmount > 0 && selectedAmount <= PARTY_ACTIVE_CAPACITY)
		{
			if (timeSincePartySelected < 0)
				timeSincePartySelected = GetTime();

			DrawPressEnterToContinue(info, HeaderSpacing::close, GetTime() - timeSincePartySelected);

			auto& gameState = info.gameState;
			gameState.partySize = selectedAmount;

			uint32_t j = 0;
			for (uint32_t i = 0; i < PARTY_CAPACITY; ++i)
			{
				if (!selected[i])
					continue;
				gameState.partyIds[j] = i;

				const auto& monster = info.monsters[info.playerState.monsterIds[i]];
				gameState.healths[j++] = monster.health;
			}

			if (info.inputState.enter.PressEvent())
				Load(LevelIndex::main);
		}
		else
			timeSincePartySelected = -1;

		return true;
	}
}
