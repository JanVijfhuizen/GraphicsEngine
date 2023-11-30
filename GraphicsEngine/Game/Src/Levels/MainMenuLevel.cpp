#include "pch_game.h"
#include "Levels/MainMenuLevel.h"
#include "CardGame.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		saveDataValid = TryLoadSaveData(info.playerState);
		inTutorial = false;
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		const glm::ivec2 origin = { 9, SIMULATED_RESOLUTION.y - 36 };
		const auto buttonOrigin = origin - glm::ivec2(-4, 36);
		constexpr uint32_t BUTTON_OFFSET = 20;

		if(!inTutorial)
		{
			HeaderDrawInfo headerDrawInfo{};
			headerDrawInfo.origin = origin;
			headerDrawInfo.text = "untitled card game";
			headerDrawInfo.lineLength = 10;
			DrawHeader(info, headerDrawInfo);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.origin = buttonOrigin;
			buttonDrawInfo.text = "new game";
			buttonDrawInfo.width = 96;
			if (DrawButton(info, buttonDrawInfo))
				Load(LevelIndex::newGame, true);

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
			buttonDrawInfo.text = "how to play";
			if (DrawButton(info, buttonDrawInfo))
			{
				inTutorial = true;
				return true;
			}

			buttonDrawInfo.origin.y -= BUTTON_OFFSET;
			buttonDrawInfo.text = "exit";
			if (DrawButton(info, buttonDrawInfo))
				return false;
			return true;
		}

		// Tutorial.
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = origin;
		headerDrawInfo.text = "how to play";
		headerDrawInfo.lineLength = 10;
		DrawHeader(info, headerDrawInfo);

		TextTask textTask{};
		textTask.position = buttonOrigin;
		textTask.text = "[mouse left] select and drag cards.";
		info.textTasks.Push(textTask);

		textTask.position.y -= BUTTON_OFFSET;
		textTask.text = "[mouse right] look at the card text.";
		info.textTasks.Push(textTask);

		textTask.position.y -= BUTTON_OFFSET;
		textTask.text = "[space] end turn.";
		info.textTasks.Push(textTask);

		textTask.position.y -= BUTTON_OFFSET;
		textTask.text = "[esc] open in game menu.";
		info.textTasks.Push(textTask);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = textTask.position;
		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = "back";
		buttonDrawInfo.width = 96;
		if (DrawButton(info, buttonDrawInfo))
			inTutorial = false;

		return true;
	}
}
