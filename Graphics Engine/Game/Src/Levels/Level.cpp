#include "pch_game.h"
#include "Levels/Level.h"

#include "GE/AtlasGenerator.h"
#include "States/InputState.h"
#include "Utils/BoxCollision.h"
#include "Utils/SubTextureUtils.h"

namespace game
{
	void Level::Create(const LevelCreateInfo& info)
	{
		_lMousePressed = false;
		_timeSinceOpened = 0;
	}

	bool Level::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		_timeSinceOpened += info.deltaTime;
		return true;
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
		
		PixelPerfectRenderTask renderTask{};
		renderTask.scale = atlasTexture.resolution / glm::ivec2(2, 1);
		renderTask.position = info.inputState.mousePos - glm::ivec2(0, renderTask.scale.y);
		renderTask.priority = true;
		renderTask.subTexture = _lMousePressed ? subTextures[1] : subTextures[0];
		info.pixelPerfectRenderTasks.Push(renderTask);
	}

	bool Level::DrawButton(const LevelUpdateInfo& info, 
		const glm::ivec2 origin, const char* text, 
		const float lifeTime, const bool reverse)
	{
		constexpr uint32_t BUTTON_ANIM_LENGTH = 6;
		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::button)];
		jv::ge::SubTexture buttonAnim[BUTTON_ANIM_LENGTH];
		Divide(buttonTexture.subTexture, buttonAnim, BUTTON_ANIM_LENGTH);

		PixelPerfectRenderTask buttonRenderTask{};
		buttonRenderTask.position = origin;
		buttonRenderTask.scale = buttonTexture.resolution / glm::ivec2(BUTTON_ANIM_LENGTH, 1);
		buttonRenderTask.subTexture = buttonAnim[5];

		TextTask buttonTextTask{};
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.position.x += 5;
		buttonTextTask.text = text;
		buttonTextTask.lifetime = lifeTime;

		const bool released = info.inputState.lMouse == InputState::released;
		bool pressed = false;
		const bool collided = CollidesShapeInt(origin, buttonRenderTask.scale, info.inputState.mousePos);
		if (collided)
		{
			buttonTextTask.loop = true;
			buttonRenderTask.color = glm::vec4(1, 0, 0, 1);
		}
		if (collided && released)
			pressed = true;

		info.textTasks.Push(buttonTextTask);
		info.pixelPerfectRenderTasks.Push(buttonRenderTask);
		return pressed;
	}

	float Level::GetTime() const
	{
		return _timeSinceOpened;
	}
}
