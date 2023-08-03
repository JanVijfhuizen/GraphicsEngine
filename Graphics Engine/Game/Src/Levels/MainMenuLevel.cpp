#include "pch_game.h"
#include "Levels/MainMenuLevel.h"

#include "CardGame.h"
#include "GE/AtlasGenerator.h"
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
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.position.x = 9;
		titleTextTask.position.y = SIMULATED_RESOLUTION.y - 36;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = 2;
		titleTextTask.lifetime = GetTime();
		info.textTasks.Push(titleTextTask);
		
		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::button)];

		PixelPerfectRenderTask newGameButtonRenderTask{};
		newGameButtonRenderTask.position.y = titleTextTask.position.y - 36;
		newGameButtonRenderTask.scale = buttonTexture.resolution;
		newGameButtonRenderTask.subTexture = buttonTexture.subTexture;

		TextTask buttonTextTask{};
		newGameButtonRenderTask.position.y = titleTextTask.position.y - 36;
		buttonTextTask.position = newGameButtonRenderTask.position;
		buttonTextTask.position.x = 9;
		buttonTextTask.text = "new game";
		buttonTextTask.lifetime = GetTime();

		const bool released = info.inputState.lMouse == InputState::released;

		{
			const bool collided = CollidesShapeInt(newGameButtonRenderTask.position, newGameButtonRenderTask.scale, info.inputState.mousePos);
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
					loadLevelIndex = LevelIndex::newGame;
			}
		}

		info.textTasks.Push(buttonTextTask);
		info.pixelPerfectRenderTasks.Push(newGameButtonRenderTask);

		if (saveDataValid)
		{
			PixelPerfectRenderTask continueButtonRenderTask{};
			continueButtonRenderTask.position.y = newGameButtonRenderTask.position.y - 18;
			continueButtonRenderTask.scale = buttonTexture.resolution;
			continueButtonRenderTask.subTexture = buttonTexture.subTexture;

			buttonTextTask.position = continueButtonRenderTask.position;
			buttonTextTask.position.x = 9;
			buttonTextTask.text = "continue";
			buttonTextTask.lifetime = GetTime();

			{
				const bool collided = CollidesShapeInt(continueButtonRenderTask.position, continueButtonRenderTask.scale, info.inputState.mousePos);
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
					else if(released && selectedContinue)
						loadLevelIndex = LevelIndex::partySelect;
				}
			}

			info.textTasks.Push(buttonTextTask);
			info.pixelPerfectRenderTasks.Push(continueButtonRenderTask);
		}

		if(released)
		{
			selectedButton = false;
			selectedNewGame = false;
			selectedContinue = false;
		}

		return true;
	}
}
