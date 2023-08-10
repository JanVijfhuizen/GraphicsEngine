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
		_timeSinceLoading = 0;
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
					_timeSinceLoading = 0;
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

	void Level::DrawHeader(const LevelUpdateInfo& info, const HeaderDrawInfo& drawInfo) const
	{
		uint32_t textMaxLen = -1;
		if(_loading)
		{
			if (_timeSinceLoading > _LOAD_DURATION)
				return;
			const float lerp = _timeSinceLoading / _LOAD_DURATION;
			const auto len = static_cast<uint32_t>(strlen(drawInfo.text));
			textMaxLen = static_cast<uint32_t>((1.f - lerp) * static_cast<float>(len));
		}

		TextTask titleTextTask{};
		titleTextTask.lineLength = drawInfo.overflow ? -1 : 10;
		titleTextTask.position = drawInfo.origin;
		titleTextTask.text = drawInfo.text;
		titleTextTask.scale = drawInfo.scale;
		titleTextTask.lifetime = _loading ? 1e2f : GetTime();
		titleTextTask.maxLength = textMaxLen;
		titleTextTask.center = drawInfo.center;
		info.textTasks.Push(titleTextTask);
	}

	bool Level::DrawButton(const LevelUpdateInfo& info, const ButtonDrawInfo& drawInfo) const
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
				const auto len = static_cast<uint32_t>(strlen(drawInfo.text));
				textMaxLen = static_cast<uint32_t>((1.f - lerp) * static_cast<float>(len));
			}
		}
		else if (_loading)
			textMaxLen = 0;

		PixelPerfectRenderTask buttonRenderTask{};
		buttonRenderTask.position = drawInfo.origin;
		buttonRenderTask.scale = buttonTexture.resolution / glm::ivec2(BUTTON_ANIM_LENGTH, 1);
		buttonRenderTask.subTexture = buttonAnim[buttonAnimIndex];
		buttonRenderTask.xCenter = drawInfo.center;

		TextTask buttonTextTask{};
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.position.x += drawInfo.center ? 0 : buttonRenderTask.scale.x / 2;
		buttonTextTask.text = drawInfo.text;
		buttonTextTask.lifetime = _loading ? 1e2f : lifeTime;
		buttonTextTask.maxLength = textMaxLen;
		buttonTextTask.center = true;

		const bool released = info.inputState.lMouse == InputState::released;
		bool pressed = false;
		const bool collided = _loading ? false : CollidesShapeInt(drawInfo.origin, buttonRenderTask.scale, info.inputState.mousePos);
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

	uint32_t Level::DrawDiscoveredCards(const LevelUpdateInfo& info, const DiscoveredCardDrawInfo& drawInfo)
	{
		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.length = 1;
		cardDrawInfo.center = true;
		cardDrawInfo.origin.x = SIMULATED_RESOLUTION.x / 2 - 32;
		cardDrawInfo.origin.y = static_cast<int32_t>(drawInfo.height);
		
		const bool released = info.inputState.lMouse == InputState::released;

		uint32_t choice = -1;
		for (uint32_t i = 0; i < DISCOVER_LENGTH; ++i)
		{
			cardDrawInfo.borderColor = drawInfo.highlighted == i ? glm::ivec4(0, 1, 0, 1) : glm::ivec4(1);
			cardDrawInfo.card = drawInfo.cards[i];
			const bool b = DrawCard(info, cardDrawInfo);
			cardDrawInfo.origin.x += 32;

			if (b && released)
				choice = i;
		}

		return choice;
	}

	bool Level::DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo)
	{
		constexpr uint32_t CARD_FRAME_COUNT = 3;

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		jv::ge::SubTexture cardFrames[CARD_FRAME_COUNT];
		Divide(cardTexture.subTexture, cardFrames, CARD_FRAME_COUNT);

		PixelPerfectRenderTask bgRenderTask{};
		bgRenderTask.position = drawInfo.origin;
		bgRenderTask.scale = cardTexture.resolution / glm::ivec2(CARD_FRAME_COUNT, 1);
		bgRenderTask.subTexture = cardFrames[0];
		bgRenderTask.xCenter = drawInfo.center;
		bgRenderTask.yCenter = drawInfo.center;
		bgRenderTask.color = drawInfo.borderColor;

		const bool collided = CollidesShapeInt(drawInfo.origin - 
			(drawInfo.center ? bgRenderTask.scale / 2 : glm::ivec2(0)), bgRenderTask.scale, info.inputState.mousePos);
		bgRenderTask.color = collided ? glm::vec4(1, 0, 0, 1) : bgRenderTask.color;
		info.pixelPerfectRenderTasks.Push(bgRenderTask);

		return collided;
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
