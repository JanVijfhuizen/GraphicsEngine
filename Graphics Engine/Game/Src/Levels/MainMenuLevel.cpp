#include "pch_game.h"
#include "Levels/MainMenuLevel.h"
#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "JLib/Curve.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		saveDataValid = TryLoadSaveData(info.playerState);
		selectedNewGame = false;
		selectedContinue = false;
		loading = false;
		timeSinceLoading = 0;
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		int32_t xMod = 0;
		if(loading)
		{
			if (timeSinceLoading >= .75f)
			{
				if (selectedNewGame)
					loadLevelIndex = LevelIndex::newGame;
				else if (selectedContinue)
					loadLevelIndex = LevelIndex::partySelect;
				return true;
			}

			const auto loadCurve = je::CreateCurveDecelerate();
			xMod = static_cast<int32_t>((1.f - loadCurve.Evaluate(1.f - timeSinceLoading / .75f)) * SIMULATED_RESOLUTION.x);
			timeSinceLoading += info.deltaTime;

			PixelPerfectRenderTask loadTask{};
			loadTask.subTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)].subTexture;
			loadTask.scale = { 3, SIMULATED_RESOLUTION.y };
			loadTask.position.x = xMod - 3;
			info.pixelPerfectRenderTasks.Push(loadTask);
		}

		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.position.x = 9 + xMod;
		titleTextTask.position.y = SIMULATED_RESOLUTION.y - 36;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = 2;
		titleTextTask.lifetime = GetTime();
		info.textTasks.Push(titleTextTask);

		glm::ivec2 buttonPos = titleTextTask.position;
		buttonPos.y -= 36;
		const bool newGameSelected = DrawButton(info, buttonPos, "new game", GetTime(), false);
		if(newGameSelected && !loading)
		{
			loading = true;
			timeSinceLoading = 0;
			selectedNewGame = true;
		}

		if (saveDataValid)
		{
			buttonPos.y -= 18;
			const bool continueSelected = DrawButton(info, buttonPos, "continue", GetTime(), false);
			if (continueSelected && !loading)
			{
				loading = true;
				timeSinceLoading = 0;
				selectedContinue = true;
			}
		}

		return true;
	}
}
