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

		constexpr glm::ivec2 headerPos{9, SIMULATED_RESOLUTION.y - 36};
		DrawHeader(info, headerPos, "untitled card game");

		glm::ivec2 buttonPos = headerPos;
		buttonPos.y -= 36;
		if(DrawButton(info, buttonPos, "new game"))
			Load(LevelIndex::newGame);

		if (saveDataValid)
		{
			buttonPos.y -= 18;
			if (DrawButton(info, buttonPos, "continue"))
				Load(LevelIndex::partySelect);
		}

		return true;
	}
}
