#include "pch_game.h"
#include "Levels/GameOverLevel.h"
#include "States/InputState.h"

namespace game
{
	bool GameOverLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y / 2 };
		headerDrawInfo.text = "game over";
		headerDrawInfo.center = true;
		headerDrawInfo.scale = 4;
		DrawHeader(info, headerDrawInfo);

		const float f = GetTime() - static_cast<float>(strlen(headerDrawInfo.text)) / TEXT_DRAW_SPEED;
		if (f >= 0)
			DrawPressEnterToContinue(info, HeaderSpacing::close, f);

		if (info.inputState.enter.PressEvent())
			loadLevelIndex = LevelIndex::mainMenu;
		return true;
	}
}
