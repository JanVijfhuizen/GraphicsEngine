#include "pch_game.h"
#include "Levels/GameOverLevel.h"
#include "States/InputState.h"
#include <Utils/SubTextureUtils.h>

namespace game
{
	bool GameOverLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		const auto& titleAtlasTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::title)];
		jv::ge::SubTexture titleFrames[4];
		Divide(titleAtlasTexture.subTexture, titleFrames, 4);

		uint32_t index = floor(fmodf(GetTime() * 6, 4));

		PixelPerfectRenderTask titleTask;
		titleTask.position = SIMULATED_RESOLUTION / 2 + glm::ivec2(0, 64);
		titleTask.scale = titleAtlasTexture.resolution / glm::ivec2(4, 1);
		titleTask.subTexture = titleFrames[index];
		titleTask.xCenter = true;
		titleTask.yCenter = true;
		titleTask.priority = true;
		info.renderTasks.Push(titleTask);

		const auto& gameOverTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::gameOver)];
		PixelPerfectRenderTask gameOverTask;
		gameOverTask.position = SIMULATED_RESOLUTION / 2 + glm::ivec2(0, 12);
		gameOverTask.scale = gameOverTexture.resolution;
		gameOverTask.subTexture = gameOverTexture.subTexture;
		gameOverTask.xCenter = true;
		gameOverTask.yCenter = true;
		info.renderTasks.Push(gameOverTask);

		DrawPressEnterToContinue(info, HeaderSpacing::normal);

		if (info.inputState.enter.PressEvent())
			loadLevelIndex = LevelIndex::mainMenu;
		return true;
	}
}
