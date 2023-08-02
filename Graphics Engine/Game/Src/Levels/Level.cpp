#include "pch_game.h"
#include "Levels/Level.h"

#include "GE/AtlasGenerator.h"
#include "States/InputState.h"
#include "Utils/SubTextureUtils.h"

namespace game
{
	void Level::Create(const LevelCreateInfo& info)
	{
		_lMousePressed = false;
	}

	void Level::PostUpdate(const LevelUpdateInfo& info)
	{
		const auto atlasTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::mouse)];

		jv::ge::SubTexture subTextures[2];
		Divide(atlasTexture.subTexture, subTextures, (sizeof subTextures) / sizeof(jv::ge::SubTexture));

		switch (info.inputState.lMouse)
		{
		case InputState::idle:
			break;
		case InputState::pressed:
			_lMousePressed = true;
			break;
		case InputState::released:
			_lMousePressed = false;
			break;
		default:
			throw std::exception("Mouse button state not supported!");
		}
		
		const auto pixelPos = PixelPerfectRenderTask::ToPixelPosition(info.resolution, SIMULATED_RESOLUTION, info.inputState.mousePos);
		PixelPerfectRenderTask renderTask{};
		renderTask.scale = atlasTexture.resolution / glm::ivec2(2, 1);
		renderTask.position = pixelPos - glm::ivec2(0, renderTask.scale.y);
		renderTask.priority = true;
		renderTask.subTexture = _lMousePressed ? subTextures[1] : subTextures[0];
		info.pixelPerfectRenderTasks.Push(renderTask);
	}
}
