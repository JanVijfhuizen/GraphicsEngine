#include "pch_game.h"
#include "Levels/MainMenuLevel.h"

#include "CardGame.h"
#include "States/InputState.h"
#include "Utils/BoxCollision.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		saveDataValid = TryLoadSaveData(info.playerState);
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		TextTask titleTextTask{};
		titleTextTask.lineLength = 10;
		titleTextTask.center = true;
		titleTextTask.position.y = -0.75f;
		titleTextTask.text = "untitled card game";
		titleTextTask.scale = .12f;
		info.textTasks.Push(titleTextTask);

		const float buttonYOffset = saveDataValid ? -.18 : 0;

		RenderTask buttonRenderTask{};
		buttonRenderTask.position.y = buttonYOffset;
		buttonRenderTask.scale.y *= .12f;
		buttonRenderTask.scale.x = .4f;
		buttonRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
		info.renderTasks.Push(buttonRenderTask);

		if (info.inputState.lMouse == InputState::pressed)
			if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
			{
				loadLevelIndex = LevelIndex::newGame;
			}

		TextTask buttonTextTask{};
		buttonTextTask.center = true;
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.text = "new game";
		buttonTextTask.scale = .06f;
		info.textTasks.Push(buttonTextTask);

		if (saveDataValid)
		{
			buttonRenderTask.position.y *= -1;
			info.renderTasks.Push(buttonRenderTask);

			buttonTextTask.position = buttonRenderTask.position;
			buttonTextTask.text = "continue";
			info.textTasks.Push(buttonTextTask);

			if (info.inputState.lMouse == InputState::pressed)
				if (CollidesShape(buttonRenderTask.position, buttonRenderTask.scale, info.inputState.mousePos))
				{
					loadLevelIndex = LevelIndex::partySelect;
				}
		}

		return true;
	}
}
