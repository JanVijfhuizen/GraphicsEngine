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
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.position.y = -0.75f;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = .12f;
		info.textTasks.Push(titleTextTask);
		
		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::button)];

		PixelPerfectRenderTask newGameButtonRenderTask{};
		newGameButtonRenderTask.position = SIMULATED_RESOLUTION / 2 + glm::ivec2(-buttonTexture.resolution.x / 2, 0);
		newGameButtonRenderTask.scale = buttonTexture.resolution;
		newGameButtonRenderTask.subTexture = buttonTexture.subTexture;
		info.pixelPerfectRenderTasks.Push(newGameButtonRenderTask);

		if (info.inputState.lMouse == InputState::pressed)
			if (CollidesShapeInt(newGameButtonRenderTask.position, newGameButtonRenderTask.scale, info.inputState.mousePos))
			{
				loadLevelIndex = LevelIndex::newGame;
			}

		TextTask buttonTextTask{};
		buttonTextTask.position = newGameButtonRenderTask.position;
		buttonTextTask.text = "new game";
		info.textTasks.Push(buttonTextTask);

		if (saveDataValid)
		{
			PixelPerfectRenderTask continueButtonRenderTask{};
			continueButtonRenderTask.position = SIMULATED_RESOLUTION / 2 - glm::ivec2(buttonTexture.resolution.x / 2, buttonTexture.resolution.y);
			continueButtonRenderTask.scale = buttonTexture.resolution;
			continueButtonRenderTask.subTexture = buttonTexture.subTexture;
			info.pixelPerfectRenderTasks.Push(continueButtonRenderTask);

			buttonTextTask.position = continueButtonRenderTask.position;
			buttonTextTask.text = "continue";
			info.textTasks.Push(buttonTextTask);

			if (info.inputState.lMouse == InputState::pressed)
				if (CollidesShapeInt(newGameButtonRenderTask.position, newGameButtonRenderTask.scale, info.inputState.mousePos))
				{
					loadLevelIndex = LevelIndex::partySelect;
				}
		}

		return true;
	}
}
