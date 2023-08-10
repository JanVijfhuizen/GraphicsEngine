#include "pch_game.h"
#include "Levels/MainMenuLevel.h"
#include "CardGame.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		saveDataValid = TryLoadSaveData(info.playerState);
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { 9, SIMULATED_RESOLUTION.y - 36 };
		headerDrawInfo.text = "untitled card game";
		DrawHeader(info, headerDrawInfo);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = headerDrawInfo.origin - glm::ivec2(0, 36);
		buttonDrawInfo.text = "new game";
		if (DrawButton(info, buttonDrawInfo))
			Load(LevelIndex::newGame);
		
		if (saveDataValid)
		{
			buttonDrawInfo.origin.y -= 18;
			buttonDrawInfo.text = "continue";
			if (DrawButton(info, buttonDrawInfo))
				Load(LevelIndex::partySelect);
		}

		return true;
	}
}
