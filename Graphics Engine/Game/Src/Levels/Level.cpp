#include "pch_game.h"
#include "Levels/Level.h"

#include "GE/AtlasGenerator.h"
#include "States/InputState.h"
#include "Utils/BoxCollision.h"
#include "Utils/SubTextureUtils.h"

namespace game
{
	constexpr uint32_t CARD_FRAME_COUNT = 3;
	constexpr uint32_t CARD_SPACING = 2;
	constexpr uint32_t CARD_STACKED_SPACING = 6;

	void Level::Create(const LevelCreateInfo& info)
	{
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
		
		PixelPerfectRenderTask renderTask{};
		renderTask.scale = atlasTexture.resolution / glm::ivec2(2, 1);
		renderTask.position = info.inputState.mousePos - glm::ivec2(0, renderTask.scale.y);
		renderTask.priority = true;
		renderTask.subTexture = info.inputState.lMouse.pressed || info.inputState.rMouse.pressed ? subTextures[1] : subTextures[0];
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
		titleTextTask.lifetime = _loading ? 1e2f : drawInfo.overrideLifeTime < 0 ? GetTime() : drawInfo.overrideLifeTime;
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
		
		bool pressed = false;
		const bool collided = _loading ? false : CollidesShapeInt(drawInfo.origin, buttonRenderTask.scale, info.inputState.mousePos);
		if (collided)
		{
			buttonTextTask.loop = true;
			buttonRenderTask.color = glm::vec4(1, 0, 0, 1);
		}
		if (collided && info.inputState.lMouse.ReleaseEvent())
			pressed = true;

		info.textTasks.Push(buttonTextTask);
		info.pixelPerfectRenderTasks.Push(buttonRenderTask);
		return pressed;
	}

	uint32_t Level::DrawCardSelection(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const uint32_t width = cardTexture.resolution.x / CARD_FRAME_COUNT;

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.length = 1;
		cardDrawInfo.center = true;
		cardDrawInfo.origin.x = static_cast<int32_t>(SIMULATED_RESOLUTION.x / 2 - (width + CARD_SPACING / 2) * (drawInfo.length - 1) / 2);
		cardDrawInfo.origin.y = static_cast<int32_t>(drawInfo.height);
		cardDrawInfo.drawBigCardIfPossible = false;
		
		const bool released = info.inputState.lMouse.pressed;

		uint32_t choice = -1;
		for (uint32_t i = 0; i < drawInfo.length; ++i)
		{
			const bool selected = drawInfo.selectedArr ? drawInfo.selectedArr[i] : drawInfo.highlighted == i;
			cardDrawInfo.borderColor = selected ? glm::ivec4(0, 1, 0, 1) : glm::ivec4(1);
			cardDrawInfo.card = drawInfo.cards[i];
			const bool b = DrawCard(info, cardDrawInfo);
			
			if (drawInfo.stacks)
			{
				uint32_t stackedSelected = -1;
				const uint32_t stackedCount = drawInfo.stackCounts[i];
				for (uint32_t j = 0; j < stackedCount; ++j)
				{
					auto stackedDrawInfo = cardDrawInfo;
					stackedDrawInfo.card = drawInfo.stacks[i][j];
					stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (stackedCount - j));
					stackedDrawInfo.drawBigCardIfPossible = !b;
					if (DrawCard(info, stackedDrawInfo) && !b)
						stackedSelected = i;
				}

				cardDrawInfo.drawBigCardIfPossible = stackedSelected == -1;
				DrawCard(info, cardDrawInfo);

				if(stackedSelected != -1)
				{
					auto stackedDrawInfo = cardDrawInfo;
					stackedDrawInfo.card = drawInfo.stacks[i][stackedSelected];
					stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (stackedCount - stackedSelected));
					stackedDrawInfo.drawBigCardIfPossible = true;
					DrawCard(info, stackedDrawInfo);
				}
			}

			cardDrawInfo.origin.x += static_cast<int32_t>(width + CARD_SPACING / 2);
			if (b && released)
				choice = i;
		}

		return choice;
	}

	bool Level::DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo)
	{
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

		if (drawInfo.drawBigCardIfPossible && collided && info.inputState.rMouse.pressed)
			DrawFullCard(info, drawInfo.card);
		return collided;
	}

	void Level::DrawFullCard(const LevelUpdateInfo& info, Card* card)
	{
		constexpr uint32_t CARD_FRAME_COUNT = 3;
		constexpr int32_t SCALE_MULTIPLIER = 4;

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		jv::ge::SubTexture cardFrames[CARD_FRAME_COUNT];
		Divide(cardTexture.subTexture, cardFrames, CARD_FRAME_COUNT);

		PixelPerfectRenderTask bgRenderTask{};
		bgRenderTask.position = SIMULATED_RESOLUTION / 2;
		bgRenderTask.scale = cardTexture.resolution / glm::ivec2(CARD_FRAME_COUNT, 1) * SCALE_MULTIPLIER;
		bgRenderTask.subTexture = cardFrames[0];
		bgRenderTask.xCenter = true;
		bgRenderTask.yCenter = true;
		bgRenderTask.priority = true;

		auto titleBoxRenderTask = bgRenderTask;
		titleBoxRenderTask.subTexture = cardFrames[1];

		auto textBoxRenderTask = bgRenderTask;
		textBoxRenderTask.subTexture = cardFrames[2];

		bgRenderTask.color = glm::ivec4(1, 0, 0, 1);
		info.pixelPerfectRenderTasks.Push(bgRenderTask);
		info.pixelPerfectRenderTasks.Push(titleBoxRenderTask);
		info.pixelPerfectRenderTasks.Push(textBoxRenderTask);

		TextTask titleTextTask{};
		titleTextTask.position = bgRenderTask.position;
		titleTextTask.position.y += bgRenderTask.scale.y / 2 - 5 * SCALE_MULTIPLIER;
		titleTextTask.text = card->name;
		titleTextTask.lifetime = 1e2f;
		titleTextTask.center = true;
		titleTextTask.priority = true;
		info.textTasks.Push(titleTextTask);

		auto ruleTextTask = titleTextTask;
		ruleTextTask.text = card->ruleText;
		ruleTextTask.position = bgRenderTask.position;
		ruleTextTask.position.y -= 7 * SCALE_MULTIPLIER;
		info.textTasks.Push(ruleTextTask);
	}

	void Level::DrawTopCenterHeader(const LevelUpdateInfo& info, const HeaderSpacing spacing, 
		const char* text, const uint32_t scale, const float overrideLifeTime) const
	{
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y - GetSpacing(spacing)};
		headerDrawInfo.text = text;
		headerDrawInfo.center = true;
		headerDrawInfo.overflow = true;
		headerDrawInfo.overrideLifeTime = overrideLifeTime;
		headerDrawInfo.scale = scale;
		DrawHeader(info, headerDrawInfo);
	}

	void Level::DrawPressEnterToContinue(const LevelUpdateInfo& info, const HeaderSpacing spacing, const float overrideLifeTime) const
	{
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, GetSpacing(spacing) };
		headerDrawInfo.text = "press enter to continue...";
		headerDrawInfo.center = true;
		headerDrawInfo.overflow = true;
		headerDrawInfo.overrideLifeTime = overrideLifeTime;
		headerDrawInfo.scale = 1;
		DrawHeader(info, headerDrawInfo);
	}

	uint32_t Level::GetSpacing(const HeaderSpacing spacing)
	{
		int32_t yOffset = 0;
		switch (spacing)
		{
		case HeaderSpacing::normal:
			yOffset = 32;
			break;
		case HeaderSpacing::close:
			yOffset = 64;
			break;
		default:
			break;
		}
		return yOffset;
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
