#include "pch_game.h"
#include "Levels/MainMenuLevel.h"
#include "CardGame.h"
#include "Interpreters/TextInterpreter.h"

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
		headerDrawInfo.lineLength = 10;
		DrawHeader(info, headerDrawInfo);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = headerDrawInfo.origin - glm::ivec2(0, 36);
		buttonDrawInfo.text = "new game";
		buttonDrawInfo.width = 96;
		if (DrawButton(info, buttonDrawInfo))
			Load(LevelIndex::newGame, true);

		constexpr uint32_t BUTTON_OFFSET = 20;

		if (saveDataValid)
		{
			buttonDrawInfo.origin.y -= BUTTON_OFFSET;
			buttonDrawInfo.text = "continue";
			if (DrawButton(info, buttonDrawInfo))
				Load(LevelIndex::partySelect, true);
		}

		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = info.isFullScreen ? "to windowed" : "to fullscreen";
		if (DrawButton(info, buttonDrawInfo))
			info.isFullScreen = !info.isFullScreen;

		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = "exit";
		if (DrawButton(info, buttonDrawInfo))
			return false;

		return true;
	}
}
