#include "pch_game.h"
#include "Levels/Level.h"

#include "GE/AtlasGenerator.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"
#include "States/BoardState.h"
#include "States/InputState.h"
#include "Utils/BoxCollision.h"
#include "Utils/SubTextureUtils.h"

namespace game
{
	constexpr uint32_t CARD_FRAME_COUNT = 2;
	constexpr uint32_t CARD_STACKED_SPACING = 8;

	bool LevelUpdateInfo::ScreenShakeInfo::IsInTimeOut() const
	{
		return remaining > -timeOut;
	}

	void Level::Create(const LevelCreateInfo& info)
	{
		_buttonHovered = false;
		_buttonHoveredLastFrame = false;
		_fullCard = nullptr;
		_timeSinceOpened = 0;
		_timeSinceLoading = 0;
		_loading = false;
		_animateLoading = false;
		for (auto& metaData : _cardDrawMetaDatas)
			metaData = {};
	}

	bool Level::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		uint32_t pixelationSteps = 0;
		auto res = SIMULATED_RESOLUTION;
		while(res.x > 1 || res.y > 1)
		{
			res /= 2;
			++pixelationSteps;
		}

		if(_loading)
		{
			_timeSinceLoading += info.deltaTime;
			info.pixelation = 1.f + (_timeSinceLoading / _LOAD_DURATION) * pixelationSteps;

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
		else
			info.pixelation = 1.f + jv::Max(0.f, 1.f - _timeSinceOpened / _LOAD_DURATION) * pixelationSteps;
		if (!_animateLoading)
			info.pixelation = 1;

		_timeSinceOpened += info.deltaTime;
		return true;
	}

	void Level::PostUpdate(const LevelUpdateInfo& info)
	{
		if (!_buttonHovered)
		{
			_buttonLifetime = 0;
			_buttonHoveredLastFrame = false;
			_buttonLifetime = 0;
		}
		else
			_buttonLifetime += info.deltaTime;
		_buttonHovered = false;

		const auto atlasTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::mouse)];

		jv::ge::SubTexture subTextures[2];
		Divide(atlasTexture.subTexture, subTextures, sizeof subTextures / sizeof(jv::ge::SubTexture));

		const auto mouseScale = atlasTexture.resolution / glm::ivec2(2, 1);
		const auto mousePos = info.inputState.mousePos - glm::ivec2(0, mouseScale.y);

		if(_selectedCard && !_fullCard)
		{
			if (!info.inputState.lMouse.pressed)
				_selectedCard = nullptr;
		}

		if(_fullCard && !_selectedCard)
		{
			constexpr float FULL_CARD_OPEN_DURATION = .1f;
			constexpr float FULL_CARD_TOTAL_DURATION = .2f;
			constexpr uint32_t FULL_CARD_LINE_LENGTH = 32;

			const float delta = info.deltaTime * (info.inputState.rMouse.pressed * 2 - 1);
			const float m = jv::Min(_fullCardLifeTime, FULL_CARD_TOTAL_DURATION);
			if (delta < 0)
				_fullCardLifeTime = m;
			_fullCardLifeTime += delta;
			if(_fullCardLifeTime < 0)
				_fullCard = nullptr;
			else
			{
				const float lerp = m / FULL_CARD_TOTAL_DURATION;
				const uint32_t lineCount = TextInterpreter::GetLineCount(_fullCard->ruleText, FULL_CARD_LINE_LENGTH);

				const auto& emptyTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)];
				const auto& alphabetTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::alphabet)];

				PixelPerfectRenderTask bgRenderTask{};
				bgRenderTask.position = SIMULATED_RESOLUTION / 2;
				bgRenderTask.scale = glm::ivec2(SIMULATED_RESOLUTION.x, lineCount * alphabetTexture.resolution.y + 6 + 13);
				bgRenderTask.scale.y *= lerp;
				bgRenderTask.subTexture = emptyTexture.subTexture;
				bgRenderTask.xCenter = true;
				bgRenderTask.yCenter = true;
				bgRenderTask.priority = true;
				info.renderTasks.Push(bgRenderTask);
				
				if(bgRenderTask.scale.y > 2)
				{
					bgRenderTask.scale.y -= 2;
					bgRenderTask.color = glm::vec4(0, 0, 0, 1);
					info.renderTasks.Push(bgRenderTask);
				}

				const float l = _fullCardLifeTime - FULL_CARD_OPEN_DURATION;
				constexpr uint32_t textOffsetX = SIMULATED_RESOLUTION.x / 8;

				CardDrawInfo cardDrawInfo{};
				cardDrawInfo.card = _fullCard;
				cardDrawInfo.origin = glm::ivec2(4, SIMULATED_RESOLUTION.y / 2);
				cardDrawInfo.origin.x += textOffsetX * lerp;
				cardDrawInfo.priority = true;
				cardDrawInfo.selectable = false;
				cardDrawInfo.scale = 2;
				//cardDrawInfo.bgColor = glm::vec4(1, 0, 0, 1);

				const char* text = _fullCard->ruleText;
				jv::LinkedList<const char*> tags{};

				const auto type = _fullCard->GetType();
				const char* cardType = "";
				switch (type)
				{
					case Card::Type::artifact:
						cardType = "[artifact] ";
						break;
					case Card::Type::curse:
						cardType = "[curse] ";
						break;
					case Card::Type::event:
						cardType = "[event] ";
						break;
					case Card::Type::monster:
						cardType = "[monster] ";
						break;
					case Card::Type::room:
						cardType = "[room] ";
						break;
					case Card::Type::spell:
						cardType = "[spell] ";
						break;
					default: 
						;
				}

				const char* tagsText = nullptr;

				if(type == Card::Type::spell)
				{
					/*
					const auto c = static_cast<SpellCard*>(_fullCard);
					cardDrawInfo.cost = c->cost;
					*/
				}
				else if (type == Card::Type::monster)
				{
					const auto c = static_cast<MonsterCard*>(_fullCard);

					if (c->tags & TAG_TOKEN)
						Add(info.frameArena, tags) = "token";
					if (c->tags & TAG_SLIME)
						Add(info.frameArena, tags) = "slime";
					if (c->tags & TAG_DRAGON)
						Add(info.frameArena, tags) = "dragon";
					if (c->tags & TAG_HUMAN)
						Add(info.frameArena, tags) = "human";
					if (c->tags & TAG_ELF)
						Add(info.frameArena, tags) = "elf";
					if (c->tags & TAG_GOBLIN)
						Add(info.frameArena, tags) = "goblin";
					if (c->tags & TAG_ELEMENTAL)
						Add(info.frameArena, tags) = "elemental";
					if (c->tags & TAG_BOSS)
						Add(info.frameArena, tags) = "boss";

					const uint32_t tagCount = tags.GetCount();
					if(tagCount > 0)
					{
						auto txt = "";
						uint32_t i = 0;
						for (const auto& tag : tags)
						{
							txt = TextInterpreter::Concat(txt, tag, info.frameArena);
							if(++i != tagCount)
								txt = TextInterpreter::Concat(txt, ", ", info.frameArena);
						}
						tagsText = txt;
					}

					cardDrawInfo.large = (c->tags & TAG_BOSS) != 0;
				}

				if (l >= 0)
				{
					const auto off = ceil(static_cast<float>(bgRenderTask.scale.y) / 2) - 2;

					TextTask titleTextTask{};
					titleTextTask.position = bgRenderTask.position;
					titleTextTask.position.x += textOffsetX;
					titleTextTask.position.y += off + alphabetTexture.resolution.y / 2;
					titleTextTask.position.y -= 17;
					titleTextTask.text = _fullCard->name;
					titleTextTask.lifetime = l * 4;
					titleTextTask.center = true;
					titleTextTask.priority = true;
					//titleTextTask.scale = 2;
					titleTextTask.largeFont = true;
					info.textTasks.Push(titleTextTask);
					titleTextTask.largeFont = false;

					if(tagsText)
					{
						auto footerTextTask = titleTextTask;
						footerTextTask.position.y = SIMULATED_RESOLUTION.y / 2 - off - alphabetTexture.resolution.y + 3;
						footerTextTask.text = tagsText;
						footerTextTask.color = glm::vec4(0, 1, 0, 1);
						footerTextTask.scale = 1;
						info.textTasks.Push(footerTextTask);

						const uint32_t footerYScale = 12 * lerp;
						auto footerBgTask = bgRenderTask;
							footerBgTask.scale.y = footerYScale;
						footerBgTask.position = footerTextTask.position - glm::ivec2(0, 2);
						footerBgTask.yCenter = false;
						footerBgTask.color = glm::vec4(1);
						info.renderTasks.Push(footerBgTask);
					}

					auto ruleTextTask = titleTextTask;
					ruleTextTask.position = bgRenderTask.position;
					ruleTextTask.position.x += textOffsetX;
					ruleTextTask.position.y -= 11;
					ruleTextTask.text = text;
					ruleTextTask.position.y += alphabetTexture.resolution.y * (lineCount - 1) / 2;
					ruleTextTask.position.y -= alphabetTexture.resolution.y / 2;
					ruleTextTask.scale = 1;
					ruleTextTask.lineLength = FULL_CARD_LINE_LENGTH;
					ruleTextTask.color = glm::vec4(1);
					info.textTasks.Push(ruleTextTask);
				}

				DrawCard(info, cardDrawInfo);
			}

			if (!info.inputState.rMouse.pressed)
				_fullCard = nullptr;
		}

		// Mouse.
		PixelPerfectRenderTask renderTask{};
		renderTask.scale = mouseScale;
		renderTask.position = mousePos;
		renderTask.front = true;
		renderTask.subTexture = info.inputState.lMouse.pressed || info.inputState.rMouse.pressed ? subTextures[1] : subTextures[0];
		info.renderTasks.Push(renderTask);

		LightTask lightTask{};
		lightTask.pos = LightTask::ToLightTaskPos(renderTask.position);
		lightTask.pos.z = .2f;
		lightTask.intensity = 4;
		lightTask.fallOf = 8;
		info.lightTasks.Push(lightTask);
	}

	void Level::DrawHeader(const LevelUpdateInfo& info, const HeaderDrawInfo& drawInfo) const
	{
		TextTask titleTextTask{};
		titleTextTask.lineLength = drawInfo.lineLength;
		titleTextTask.position = drawInfo.origin;
		titleTextTask.text = drawInfo.text;
		titleTextTask.scale = drawInfo.scale;
		titleTextTask.lifetime = drawInfo.overrideLifeTime < 0 ? GetTime() : drawInfo.overrideLifeTime;
		titleTextTask.center = drawInfo.center;
		info.textTasks.Push(titleTextTask);
	}

	bool Level::DrawButton(const LevelUpdateInfo& info, const ButtonDrawInfo& drawInfo, float overrideLifetime)
	{
		constexpr float BUTTON_SPAWN_ANIM_DURATION = .4f;

		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)];
		uint32_t textMaxLen = -1;

		const float lifeTime = overrideLifetime < -1e-5f ? GetTime() : overrideLifetime;
		float l = 1;
		if (lifeTime <= BUTTON_SPAWN_ANIM_DURATION)
			l = lifeTime / BUTTON_SPAWN_ANIM_DURATION;

		PixelPerfectRenderTask buttonRenderTask{};
		buttonRenderTask.position = drawInfo.origin;
		buttonRenderTask.scale = glm::ivec2(drawInfo.width, 2);
		buttonRenderTask.subTexture = buttonTexture.subTexture;
		buttonRenderTask.xCenter = drawInfo.center;
		buttonRenderTask.priority = drawInfo.priority;

		TextTask buttonTextTask{};
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.position.x += drawInfo.center ? -buttonRenderTask.scale.x / 2 : 0;
		buttonTextTask.position.x += drawInfo.centerText ? buttonRenderTask.scale.x / 2 : 0;
		buttonTextTask.position.y += 3;
		buttonTextTask.text = drawInfo.text;
		buttonTextTask.lifetime = lifeTime;
		buttonTextTask.maxLength = textMaxLen;
		buttonTextTask.center = drawInfo.centerText;
		buttonTextTask.largeFont = drawInfo.largeFont;
		buttonRenderTask.priority = drawInfo.priority;

		buttonRenderTask.scale.x *= l;
		bool pressed = false;
		const bool collided = _loading ? false : CollidesShapeInt(drawInfo.origin - 
			glm::ivec2(buttonRenderTask.scale.x / 2, 0) * static_cast<int32_t>(drawInfo.center),
			buttonRenderTask.scale + glm::ivec2(0, drawInfo.largeFont ? 17 : 9), info.inputState.mousePos);
		if (collided)
		{
			buttonTextTask.loop = true;
			buttonRenderTask.color = glm::vec4(1, 0, 0, 1);
			if(drawInfo.showLine)
				info.renderTasks.Push(buttonRenderTask);
			//buttonTextTask.lifetime = _buttonLifetime;
			//buttonTextTask.fadeIn = false;
			_buttonHovered = true;
		}
		else if(drawInfo.showLine && drawInfo.drawLineByDefault)
			info.renderTasks.Push(buttonRenderTask);
		if (collided && info.inputState.lMouse.ReleaseEvent())
			pressed = true;

		info.textTasks.Push(buttonTextTask);
		return pressed;
	}

	glm::ivec2 Level::GetCardShape(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const uint32_t width = cardTexture.resolution.x / CARD_FRAME_COUNT + drawInfo.offsetMod;
		return {width, cardTexture.resolution.y };
	}

	glm::ivec2 Level::GetCardPosition(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo,
		const uint32_t i)
	{
		auto origin = glm::ivec2(0, drawInfo.height);
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const uint32_t width = cardTexture.resolution.x / CARD_FRAME_COUNT + drawInfo.offsetMod;

		const uint32_t m = jv::Min(drawInfo.length, drawInfo.rowCutoff);
		origin.x = static_cast<int32_t>(SIMULATED_RESOLUTION.x / 2 - width * (m - 1) / 2);
		origin.x += static_cast<int32_t>(width) * (i % drawInfo.rowCutoff);
		if (i != 0)
			origin.y -= (cardTexture.resolution.y + 4) * (i / drawInfo.rowCutoff);
		origin.x += drawInfo.centerOffset;
		return origin;
	}

	uint32_t Level::DrawCardSelection(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		const auto shape = GetCardShape(info, drawInfo);

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.center = true;
		
		uint32_t choice = -1;
		if(drawInfo.outStackSelected)
			*drawInfo.outStackSelected = -1;

		uint32_t xOffset = 0;

		if(drawInfo.spawning && drawInfo.length > 1)
		{
			const auto w = shape.x;
			const auto curve = je::CreateCurveOvershooting();
			const float eval = curve.REvaluate(drawInfo.spawnLerp);

			xOffset = (1.f - eval) * w / 2;
		}

		for (uint32_t i = 0; i < drawInfo.length; ++i)
		{
			int32_t xAddOffset = 0;
			int32_t yAddOffset = 0;

			const uint32_t scale = drawInfo.containsBoss && i == 0 ? 2 : 1;
			
			cardDrawInfo.activationLerp = drawInfo.activationIndex == i ? drawInfo.activationLerp : -1;
			cardDrawInfo.lifeTime = drawInfo.lifeTime;
			cardDrawInfo.priority = false;
			cardDrawInfo.target = drawInfo.targets ? drawInfo.targets[i] : -1;
			cardDrawInfo.large = drawInfo.containsBoss && i == 0;
			cardDrawInfo.scale = scale;

			if(drawInfo.damagedIndex == i)
			{
				if (fmodf(drawInfo.lifeTime, .2f) < .1f)
					continue;
			}
			
			if(drawInfo.spawning && i == drawInfo.length - 1)
			{
				const auto w = shape.x;
				const auto curve = je::CreateCurveOvershooting();
				const float eval = curve.REvaluate(drawInfo.spawnLerp);

				//xAddOffset = (1.f - eval) * w / 8 * (drawInfo.spawnRight * 2 - 1);
				yAddOffset = DoubleCurveEvaluate(drawInfo.spawnLerp, curve, curve) * shape.y / 4;
				cardDrawInfo.lifeTime = drawInfo.spawnLerp * CARD_FADE_DURATION;
			}

			if(drawInfo.fadeIndex != -1)
			{
				const bool fading = drawInfo.fadeLerp < .5f;
				if (drawInfo.fadeIndex == i && !fading)
					continue;
				if (drawInfo.length > 1 && !fading)
				{
					const auto w = GetCardShape(info, drawInfo).x;
					const auto curve = je::CreateCurveOvershooting();
					const float eval = curve.REvaluate((drawInfo.fadeLerp - .5f) * 2);

					float o = eval / 2.f * w;
					if (drawInfo.fadeIndex < i)
						o *= -1;
					xAddOffset += o;
				}
			}

			cardDrawInfo.origin = drawInfo.overridePosIndex == i ? drawInfo.overridePos : GetCardPosition(info, drawInfo, i);
			cardDrawInfo.origin.x += xOffset;
			cardDrawInfo.origin.x += xAddOffset;
			cardDrawInfo.origin.y += yAddOffset;

			const bool greyedOut = drawInfo.greyedOutArr ? drawInfo.greyedOutArr[i] : false;
			const bool selected = drawInfo.selectedArr ? drawInfo.selectedArr[i] : drawInfo.highlighted == i;

			cardDrawInfo.fgColor = glm::vec4(1);
			cardDrawInfo.bgColor = glm::vec4(0, 0, 0, 1);
			cardDrawInfo.fgColor = greyedOut ? glm::vec4(glm::vec3(.02), 1) : cardDrawInfo.fgColor;
			cardDrawInfo.fgColor = selected ? glm::vec4(0, 1, 0, 1) : cardDrawInfo.fgColor;

			if (drawInfo.fadeIndex == i)
			{
				const auto mul = glm::vec4(glm::vec3(jv::Max(1.f - drawInfo.fadeLerp * 2, 0.f)), 1);
				cardDrawInfo.bgColor *= mul;
				cardDrawInfo.fgColor *= mul;
			}

			cardDrawInfo.card = drawInfo.cards[i];
			cardDrawInfo.selectable = drawInfo.selectable;
			const bool collides = CollidesCard(info, cardDrawInfo);
			uint32_t stackedSelected = -1;
			uint32_t stackedCount = -1;

			const bool dragged = drawInfo.selectable && drawInfo.draggable && drawInfo.highlighted == i;
			if (drawInfo.draggable && dragged)
			{
				if (dragged && drawInfo.metaDatas)
				{
					auto& metaData = drawInfo.metaDatas[i];
					if (!metaData.dragging)
					{
						metaData.mouseOffset = info.inputState.mousePos - cardDrawInfo.origin;
						metaData.dragging = true;
					}
					cardDrawInfo.priority = true;
				}
				if (drawInfo.metaDatas)
				{
					auto& metaData = drawInfo.metaDatas[i];
					if (dragged)
						cardDrawInfo.origin = info.inputState.mousePos - metaData.mouseOffset;
					else
						metaData.dragging = false;
				}
			}
			else if(drawInfo.metaDatas)
				drawInfo.metaDatas[i].dragging = false;

			cardDrawInfo.ignoreAnim = true;
			cardDrawInfo.metaData = nullptr;

			constexpr uint32_t stackWidth = 8;
			
			if (drawInfo.stacks)
			{
				stackedCount = drawInfo.stackCounts[i];

				if(!collides)
					for (uint32_t j = 0; j < stackedCount; ++j)
					{
						auto stackedDrawInfo = cardDrawInfo;
						stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (j + 1));
						stackedDrawInfo.origin.x += stackWidth * (j % 2 == 0);
						if(!dragged && CollidesCard(info, stackedDrawInfo))
						{
							stackedSelected = stackedCount - j - 1;
							break;
						}
					}
				
				for (uint32_t j = 0; j < stackedCount; ++j)
				{
					auto stackedDrawInfo = cardDrawInfo;
					stackedDrawInfo.card = drawInfo.stacks[i][j];
					stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (stackedCount - j));
					stackedDrawInfo.origin.x += stackWidth * ((stackedCount - j - 1) % 2 == 0);
					stackedDrawInfo.selectable = drawInfo.selectable && !dragged && !collides && stackedSelected == j;
					DrawCard(info, stackedDrawInfo);
				}
			}

			if(drawInfo.redHighlight - 1 == i)
				cardDrawInfo.bgColor = glm::vec4(1, 0, 0, 1);
			if(drawInfo.combatStats)
				cardDrawInfo.combatStats = &drawInfo.combatStats[i];
			cardDrawInfo.ignoreAnim = stackedSelected != -1;
			if (drawInfo.costs)
				cardDrawInfo.cost = drawInfo.costs[i];
			if(drawInfo.metaDatas && !stackedSelected != -1)
				cardDrawInfo.metaData = &drawInfo.metaDatas[i];
			DrawCard(info, cardDrawInfo);
			cardDrawInfo.combatStats = nullptr;
			cardDrawInfo.cost = -1;

			if (stackedSelected != -1)
			{
				auto stackedDrawInfo = cardDrawInfo;
				stackedDrawInfo.card = drawInfo.stacks[i][stackedSelected];
				stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (stackedCount - stackedSelected));
				stackedDrawInfo.ignoreAnim = false;
				stackedDrawInfo.metaData = nullptr;
				stackedDrawInfo.origin.x += stackWidth * ((stackedCount - stackedSelected - 1) % 2 == 0);
				DrawCard(info, stackedDrawInfo);
				cardDrawInfo.selectable = false;
			}

			if (drawInfo.outStackSelected && stackedSelected != -1)
				*drawInfo.outStackSelected = stackedSelected;

			if(drawInfo.texts && drawInfo.texts[i])
			{
				TextTask textTask{};
				textTask.position = cardDrawInfo.origin;
				textTask.position.y += cardTexture.resolution.y / 2 + 4;
				if (stackedCount != -1)
					textTask.position.y += static_cast<int32_t>(CARD_STACKED_SPACING * stackedCount);
				textTask.text = drawInfo.texts[i];
				textTask.lifetime = cardDrawInfo.lifeTime;
				textTask.center = true;
				info.textTasks.Push(textTask);
			}

			if ((collides || stackedSelected != -1) && !greyedOut)
				choice = i;
		}

		return drawInfo.selectable ? choice : -1;
	}

	bool Level::DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		jv::ge::SubTexture cardFrames[CARD_FRAME_COUNT];
		Divide(cardTexture.subTexture, cardFrames, CARD_FRAME_COUNT);

		auto origin = drawInfo.origin;
		if (drawInfo.metaData)
		{
			const auto curve = je::CreateCurveOvershooting();
			origin.y += curve.Evaluate(drawInfo.metaData->hoverDuration) * 8 * drawInfo.scale;
		}

		glm::vec3 fadeMod{1};
		if(CARD_FADE_DURATION > drawInfo.lifeTime && drawInfo.lifeTime > -1e-5f)
		{
			const auto curve = je::CreateCurveOvershooting();
			const float eval = curve.Evaluate(drawInfo.lifeTime / CARD_FADE_DURATION);
			fadeMod *= eval;
		}

		PixelPerfectRenderTask bgRenderTask{};
		bgRenderTask.scale = cardTexture.resolution / glm::ivec2(CARD_FRAME_COUNT, 1);
		bgRenderTask.scale *= drawInfo.scale;
		bgRenderTask.subTexture = cardFrames[0];
		bgRenderTask.xCenter = drawInfo.center;
		bgRenderTask.yCenter = drawInfo.center;
		bgRenderTask.priority = drawInfo.priority;

		const bool collided = drawInfo.selectable ? CollidesShapeInt(drawInfo.origin - 
			(drawInfo.center ? bgRenderTask.scale / 2 : glm::ivec2(0)), bgRenderTask.scale, info.inputState.mousePos) : false;
		bgRenderTask.color = drawInfo.card ? drawInfo.bgColor : drawInfo.fgColor * .4f;
		bgRenderTask.color = collided && drawInfo.selectable ? glm::vec4(1, 0, 0, 1) : bgRenderTask.color;

		if (drawInfo.metaData)
		{
			drawInfo.metaData->hoverDuration += 5 * info.deltaTime * ((collided || drawInfo.activationLerp >= 0) * 2 - 1);
			drawInfo.metaData->hoverDuration = jv::Clamp(drawInfo.metaData->hoverDuration, 0.f, 1.f);

			if(drawInfo.metaData->hoverDuration > 1e-5f)
				bgRenderTask.color = glm::vec4(drawInfo.metaData->hoverDuration, 0, 0, 1);
		}

		bgRenderTask.position = origin;
		bgRenderTask.color *= glm::vec4(fadeMod, 1);
		info.renderTasks.Push(bgRenderTask);

		PixelPerfectRenderTask fgRenderTask = bgRenderTask;
		fgRenderTask.subTexture = cardFrames[1];
		fgRenderTask.color = drawInfo.fgColor * glm::vec4(fadeMod, 1);
		info.renderTasks.Push(fgRenderTask);

		// Draw image.
		if (!drawInfo.ignoreAnim && drawInfo.card)
		{
			PixelPerfectRenderTask imageRenderTask{};
			imageRenderTask.position = origin;

			uint32_t frameCount;

			if(!drawInfo.large)
			{
				imageRenderTask.normalImage = info.textureStreamer.Get(drawInfo.card->normalAnimIndex, &frameCount);
				imageRenderTask.image = info.textureStreamer.Get(drawInfo.card->animIndex, &frameCount);
			}
			else
			{
				imageRenderTask.normalImage = info.largeTextureStreamer.Get(drawInfo.card->normalAnimIndex, &frameCount);
				imageRenderTask.image = info.largeTextureStreamer.Get(drawInfo.card->animIndex, &frameCount);
			}

			auto i = static_cast<uint32_t>(GetTime() * CARD_ANIM_SPEED);
			i %= frameCount;
			
			const uint32_t ml = drawInfo.large ? LARGE_CARD_ART_MAX_LENGTH : CARD_ART_MAX_LENGTH;

			jv::ge::SubTexture animFrames[CARD_ART_MAX_LENGTH > LARGE_CARD_ART_MAX_LENGTH ? CARD_ART_MAX_LENGTH : LARGE_CARD_ART_MAX_LENGTH];
			Divide({}, animFrames, ml);
			
			imageRenderTask.scale = CARD_ART_SHAPE;
			imageRenderTask.scale *= drawInfo.scale;
			imageRenderTask.xCenter = drawInfo.center;
			imageRenderTask.yCenter = drawInfo.center;
			imageRenderTask.subTexture = animFrames[i];
			imageRenderTask.color *= glm::vec4(fadeMod, 1);
			imageRenderTask.priority = drawInfo.priority;

			// Hover anim.
			if(drawInfo.metaData && !drawInfo.large)
				if(drawInfo.metaData->hoverDuration > 0)
				{
					const auto c = je::CreateCurveOvershooting();
					const auto c2 = je::CreateCurveDecelerate();
					const float eval = DoubleCurveEvaluate(fmodf(drawInfo.lifeTime, 1), c, c2);
					imageRenderTask.position.y += eval * 2;
				}

			const uint32_t shadowLerpDis = 16 * drawInfo.scale;
			const glm::vec2 off = origin - info.inputState.mousePos;
			const float dis = length(off);
			const float shadowLerp = dis > shadowLerpDis ? 0 : 1.f - dis / shadowLerpDis;

			auto shadowTask = imageRenderTask;
			shadowTask.color = glm::vec4(0, 0, 0, 1);
			shadowTask.position += off * shadowLerp;
			
			info.renderTasks.Push(shadowTask);
			info.renderTasks.Push(imageRenderTask);
		}

		const bool priority = drawInfo.priority || !(info.inputState.rMouse.pressed || info.inputState.lMouse.pressed);
		const auto& statsTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::stats)];
		jv::ge::SubTexture statFrames[3];
		Divide(statsTexture.subTexture, statFrames, 3);

		float textLifeTime = drawInfo.lifeTime;
		if (drawInfo.metaData && drawInfo.metaData->timeSinceStatsChanged > 0)
			textLifeTime -= drawInfo.metaData->timeSinceStatsChanged;

		if(drawInfo.cost != -1)
		{
			const auto& manaCrystalTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::manacrystal)];

			PixelPerfectRenderTask costRenderTask{};
			costRenderTask.position = origin + glm::ivec2(0, bgRenderTask.scale.y / 2 + 10 * drawInfo.scale);
			costRenderTask.position.y -= 16 * drawInfo.scale;
			costRenderTask.scale = manaCrystalTexture.resolution;
			costRenderTask.scale *= drawInfo.scale;
			costRenderTask.xCenter = drawInfo.center;
			costRenderTask.yCenter = drawInfo.center;
			costRenderTask.color = fgRenderTask.color;
			costRenderTask.subTexture = manaCrystalTexture.subTexture;
			costRenderTask.priority = priority;
			info.renderTasks.Push(costRenderTask);

			TextTask textTask{};
			textTask.position = costRenderTask.position;
			textTask.center = drawInfo.center;
			textTask.position.y -= 2 * drawInfo.scale;
			textTask.text = TextInterpreter::IntToConstCharPtr(drawInfo.cost, info.frameArena);
			textTask.priority = priority;
			textTask.lifetime = textLifeTime;
			textTask.scale *= drawInfo.scale;
			info.textTasks.Push(textTask);
		}

		if (drawInfo.combatStats)
		{
			auto combatStats = *drawInfo.combatStats;

			PixelPerfectRenderTask statsRenderTask{};
			statsRenderTask.position = origin;
			statsRenderTask.position.x -= bgRenderTask.scale.x / 2 + 6 * drawInfo.scale;
			statsRenderTask.position.y -= 4 * drawInfo.scale;
			statsRenderTask.scale = statsTexture.resolution / glm::ivec2(3, 1);
			statsRenderTask.scale *= drawInfo.scale;
			statsRenderTask.xCenter = drawInfo.center;
			statsRenderTask.yCenter = drawInfo.center;
			statsRenderTask.color *= glm::vec4(fadeMod, 1);
			statsRenderTask.priority = priority;
			
			uint32_t values[3]
			{
				combatStats.attack,
				combatStats.health,
				drawInfo.target
			};

			uint32_t tempValues[3]
			{
				combatStats.tempAttack,
				combatStats.tempHealth,
				0
			};

			for (uint32_t i = 0; i < 3; ++i)
			{
				if (i == 2 && drawInfo.target == -1)
					continue;

				statsRenderTask.subTexture = statFrames[i];
				statsRenderTask.position.y -= statsRenderTask.scale.y;
				info.renderTasks.Push(statsRenderTask);
				
				TextTask textTask{};
				textTask.position = statsRenderTask.position + glm::ivec2(2 * drawInfo.scale, -statsRenderTask.scale.y / 2);
				textTask.text = TextInterpreter::IntToConstCharPtr(values[i], info.frameArena);
				if(tempValues[i] != 0)
				{
					textTask.text = TextInterpreter::Concat(textTask.text, "+", info.frameArena);
					const auto c = TextInterpreter::IntToConstCharPtr(tempValues[i], info.frameArena);
					textTask.text = TextInterpreter::Concat(textTask.text, c, info.frameArena);
					textTask.color = glm::vec4(0, 1, 0, 1);
				}

				textTask.lifetime = textLifeTime;
				textTask.priority = priority;
				textTask.scale *= drawInfo.scale;
				info.textTasks.Push(textTask);
			}
		}

		if (drawInfo.selectable && collided && info.inputState.rMouse.PressEvent())
			DrawFullCard(drawInfo.card);
		return collided;
	}

	bool Level::CollidesCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		jv::ge::SubTexture cardFrames[CARD_FRAME_COUNT];
		Divide(cardTexture.subTexture, cardFrames, CARD_FRAME_COUNT);
		
		const auto scale = cardTexture.resolution / glm::ivec2(CARD_FRAME_COUNT, 1);
		const bool collided = CollidesShapeInt(drawInfo.origin -
			(drawInfo.center ? scale / 2 : glm::ivec2(0)), scale, info.inputState.mousePos);
		return collided;
	}

	void Level::DrawFullCard(Card* card)
	{
		if (card == nullptr)
			_fullCard = nullptr;
		if (_fullCard)
			return;
		_fullCard = card;
		_fullCardLifeTime = 0;
	}

	void Level::DrawSelectedCard(Card* card)
	{
		if (card == nullptr)
			_selectedCard = nullptr;
		if (_selectedCard)
			return;
		_selectedCard = card;
		_selectedCardLifeTime = 0;
	}

	void Level::DrawTopCenterHeader(const LevelUpdateInfo& info, const HeaderSpacing spacing, 
		const char* text, const uint32_t scale, const float overrideLifeTime) const
	{
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, SIMULATED_RESOLUTION.y - GetSpacing(spacing)};
		headerDrawInfo.text = text;
		headerDrawInfo.center = true;
		headerDrawInfo.overrideLifeTime = overrideLifeTime;
		headerDrawInfo.scale = scale;
		DrawHeader(info, headerDrawInfo);
	}

	void Level::DrawPressEnterToContinue(const LevelUpdateInfo& info, const HeaderSpacing spacing, const float overrideLifeTime) const
	{
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = { SIMULATED_RESOLUTION.x / 2, GetSpacing(spacing) };
		headerDrawInfo.text = "press space to continue...";
		headerDrawInfo.center = true;
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
		case HeaderSpacing::far:
			yOffset = 12;
			break;
		default:
			break;
		}
		return yOffset;
	}

	CombatStats Level::GetCombatStat(const MonsterCard& card)
	{
		CombatStats info{};
		info.attack = card.attack;
		info.health = card.health;
		return info;
	}

	float Level::GetTime() const
	{
		return _timeSinceOpened;
	}

	bool Level::GetIsLoading() const
	{
		return _loading;
	}

	void Level::Load(const LevelIndex index, bool animate)
	{
		if (_loading)
			return;
		_loading = true;
		_loadingLevelIndex = index;
		_timeSinceLoading = 0;
		_animateLoading = animate;
	}
}
