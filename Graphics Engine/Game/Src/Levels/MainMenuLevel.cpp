#include "pch_game.h"
#include "Levels/MainMenuLevel.h"

#include "CardGame.h"
#include "GE/AtlasGenerator.h"
#include "JLib/Curve.h"
#include "States/InputState.h"
#include "Utils/BoxCollision.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		saveDataValid = TryLoadSaveData(info.playerState);
		selectedButton = false;
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
		
		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::button)];

		PixelPerfectRenderTask newGameButtonRenderTask{};
		newGameButtonRenderTask.position.y = titleTextTask.position.y - 36;
		newGameButtonRenderTask.position.x = xMod;
		newGameButtonRenderTask.scale = buttonTexture.resolution;
		newGameButtonRenderTask.subTexture = buttonTexture.subTexture;

		TextTask buttonTextTask{};
		buttonTextTask.position = newGameButtonRenderTask.position;
		buttonTextTask.position.x = 9 + xMod;
		buttonTextTask.text = "new game";
		buttonTextTask.lifetime = GetTime();

		const bool released = info.inputState.lMouse == InputState::released;

		{
			const bool collided = loading ? false : CollidesShapeInt(newGameButtonRenderTask.position, newGameButtonRenderTask.scale, info.inputState.mousePos);
			if(collided || selectedNewGame)
			{
				buttonTextTask.loop = true;
				newGameButtonRenderTask.color = glm::vec4(1, 0, 0, 1);
			}
			if (collided)
			{
				if (info.inputState.lMouse == InputState::pressed)
				{
					selectedNewGame = true;
					selectedButton = true;
				}
				else if (released && selectedNewGame)
				{
					loading = true;
					timeSinceLoading = 0;
				}
			}
		}

		info.textTasks.Push(buttonTextTask);
		info.pixelPerfectRenderTasks.Push(newGameButtonRenderTask);

		if (saveDataValid)
		{
			PixelPerfectRenderTask continueButtonRenderTask{};
			continueButtonRenderTask.position.y = newGameButtonRenderTask.position.y - 18;
			continueButtonRenderTask.position.x = xMod;
			continueButtonRenderTask.scale = buttonTexture.resolution;
			continueButtonRenderTask.subTexture = buttonTexture.subTexture;

			buttonTextTask.position = continueButtonRenderTask.position;
			buttonTextTask.position.x = 9 + xMod;
			buttonTextTask.text = "continue";
			buttonTextTask.lifetime = GetTime();

			{
				const bool collided = loading ? false : CollidesShapeInt(continueButtonRenderTask.position, continueButtonRenderTask.scale, info.inputState.mousePos);
				if (collided || selectedContinue)
				{
					continueButtonRenderTask.color = glm::vec4(1, 0, 0, 1);
					buttonTextTask.loop = true;
				}
				if(collided)
				{
					if (info.inputState.lMouse == InputState::pressed)
					{
						selectedContinue = true;
						selectedButton = true;
					}
					else if (released && selectedContinue)
					{
						loading = true;
						timeSinceLoading = 0;
					}
				}
			}

			info.textTasks.Push(buttonTextTask);
			info.pixelPerfectRenderTasks.Push(continueButtonRenderTask);
		}

		if(released && !loading)
		{
			selectedButton = false;
			selectedNewGame = false;
			selectedContinue = false;
		}

		return true;
	}
}
