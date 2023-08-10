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
		_loading = false;
	}

	bool Level::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if(_loading)
		{
			_timeSinceLoading += info.deltaTime;
			if (_timeSinceLoading > _LOAD_DURATION)
			{
				if(_loadingLevelIndex == LevelIndex::animOnly)
				{
					_lMousePressed = false;
					_timeSinceOpened = 0;
					_loading = false;
					return true;
				}

				loadLevelIndex = _loadingLevelIndex;
				return true;
			}
		}

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

	void Level::DrawHeader(const LevelUpdateInfo& info, 
		const glm::ivec2 origin, const char* text, 
		const bool center, bool overflow) const
	{
		uint32_t textMaxLen = -1;
		if(_loading)
		{
			if (_timeSinceLoading > _LOAD_DURATION)
				return;
			const float lerp = _timeSinceLoading / _LOAD_DURATION;
			const auto len = static_cast<uint32_t>(strlen(text));
			textMaxLen = static_cast<uint32_t>((1.f - lerp) * static_cast<float>(len));
		}

		TextTask titleTextTask{};
		titleTextTask.lineLength = overflow ? -1 : 10;
		titleTextTask.position = origin;
		titleTextTask.text = text;
		titleTextTask.scale = 2;
		titleTextTask.lifetime = _loading ? 1e2f : GetTime();
		titleTextTask.maxLength = textMaxLen;
		titleTextTask.center = center;
		info.textTasks.Push(titleTextTask);
	}

	bool Level::DrawButton(const LevelUpdateInfo& info, 
		const glm::ivec2 origin, const char* text, const bool center) const
	{
		constexpr uint32_t BUTTON_ANIM_LENGTH = 6;
		constexpr float BUTTON_SPAWN_ANIM_DURATION = .4f;

		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::button)];
		jv::ge::SubTexture buttonAnim[BUTTON_ANIM_LENGTH];
		Divide(buttonTexture.subTexture, buttonAnim, BUTTON_ANIM_LENGTH);

		uint32_t buttonAnimIndex = _loading ? 0 : BUTTON_ANIM_LENGTH - 1;
		uint32_t textMaxLen = -1;

		const float lifeTime = _loading ? _timeSinceLoading : GetTime();
		if (lifeTime <= BUTTON_SPAWN_ANIM_DURATION)
		{
			const float lerp = lifeTime / BUTTON_SPAWN_ANIM_DURATION;
			buttonAnimIndex = static_cast<uint32_t>(lerp * BUTTON_ANIM_LENGTH);
			if (_loading)
			{
				buttonAnimIndex = BUTTON_ANIM_LENGTH - buttonAnimIndex - 1;
				const auto len = static_cast<uint32_t>(strlen(text));
				textMaxLen = static_cast<uint32_t>((1.f - lerp) * static_cast<float>(len));
			}
		}
		else if (_loading)
			textMaxLen = 0;

		PixelPerfectRenderTask buttonRenderTask{};
		buttonRenderTask.position = origin;
		buttonRenderTask.scale = buttonTexture.resolution / glm::ivec2(BUTTON_ANIM_LENGTH, 1);
		buttonRenderTask.subTexture = buttonAnim[buttonAnimIndex];
		buttonRenderTask.xCenter = center;

		TextTask buttonTextTask{};
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.position.x += center ? 0 : buttonRenderTask.scale.x / 2;
		buttonTextTask.text = text;
		buttonTextTask.lifetime = _loading ? 1e2f : lifeTime;
		buttonTextTask.maxLength = textMaxLen;
		buttonTextTask.center = true;

		const bool released = info.inputState.lMouse == InputState::released;
		bool pressed = false;
		const bool collided = _loading ? false : CollidesShapeInt(origin, buttonRenderTask.scale, info.inputState.mousePos);
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

	bool Level::GetIsLoading() const
	{
		return _loading;
	}

	void Level::Load(const LevelIndex index)
	{
		_loading = true;
		_loadingLevelIndex = index;
		_timeSinceLoading = 0;
	}
}
