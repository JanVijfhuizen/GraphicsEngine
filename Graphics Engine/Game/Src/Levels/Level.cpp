#include "pch_game.h"
#include "Levels/Level.h"

#include "GE/AtlasGenerator.h"
#include "Interpreters/TextInterpreter.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"
#include "States/BoardState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Utils/BoxCollision.h"
#include "Utils/SubTextureUtils.h"

namespace game
{
	constexpr uint32_t CARD_FRAME_COUNT = 4;
	constexpr uint32_t CARD_STACKED_SPACING = 6;
	constexpr float CARD_MONSTER_ANIM_SPEED = .5f;

	void Level::Create(const LevelCreateInfo& info)
	{
		_fullCard = nullptr;
		_timeSinceOpened = 0;
		_timeSinceLoading = 0;
		_loading = false;
		for (auto& hoverDuration : _hoverDurations)
			hoverDuration = 0;
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

		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::cardLarge)];

		if(_fullCard)
		{
			PixelPerfectRenderTask bgRenderTask{};
			bgRenderTask.position = SIMULATED_RESOLUTION / 2;
			bgRenderTask.scale = cardTexture.resolution;
			bgRenderTask.subTexture = cardTexture.subTexture;
			bgRenderTask.xCenter = true;
			bgRenderTask.yCenter = true;
			bgRenderTask.priority = true;

			bgRenderTask.color = glm::ivec4(1, 0, 0, 1);
			info.pixelPerfectRenderTasks.Push(bgRenderTask);

			TextTask titleTextTask{};
			titleTextTask.position = bgRenderTask.position;
			titleTextTask.position.y += bgRenderTask.scale.y / 2 - 28;
			titleTextTask.text = _fullCard ? _fullCard->name : "empty slot";
			titleTextTask.lifetime = 1e2f;
			titleTextTask.center = true;
			titleTextTask.priority = true;
			titleTextTask.lineLength = 18;
			info.textTasks.Push(titleTextTask);

			auto ruleTextTask = titleTextTask;
			ruleTextTask.position = bgRenderTask.position;
			ruleTextTask.position.y -= 36;
			ruleTextTask.text = _fullCard ? _fullCard->ruleText : "...";
			info.textTasks.Push(ruleTextTask);

			if (!info.inputState.rMouse.pressed)
				_fullCard = nullptr;
		}

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
		titleTextTask.lineLength = drawInfo.lineLength;
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
		constexpr float BUTTON_SPAWN_ANIM_DURATION = .4f;

		const auto& buttonTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::empty)];
		uint32_t textMaxLen = -1;

		const float lifeTime = _loading ? _timeSinceLoading : GetTime();
		if (lifeTime <= BUTTON_SPAWN_ANIM_DURATION)
		{
			const float lerp = lifeTime / BUTTON_SPAWN_ANIM_DURATION;
			if (_loading)
			{
				const auto len = static_cast<uint32_t>(strlen(drawInfo.text));
				textMaxLen = static_cast<uint32_t>((1.f - lerp) * static_cast<float>(len));
			}
		}
		else if (_loading)
			textMaxLen = 0;

		PixelPerfectRenderTask buttonRenderTask{};
		buttonRenderTask.position = drawInfo.origin;
		buttonRenderTask.scale = glm::ivec2(64, 2);
		buttonRenderTask.subTexture = buttonTexture.subTexture;
		buttonRenderTask.xCenter = drawInfo.center;

		TextTask buttonTextTask{};
		buttonTextTask.position = buttonRenderTask.position;
		buttonTextTask.position.x += drawInfo.center ? 0 : buttonRenderTask.scale.x / 2;
		buttonTextTask.position.y += 3;
		buttonTextTask.text = drawInfo.text;
		buttonTextTask.lifetime = _loading ? 1e2f : lifeTime;
		buttonTextTask.maxLength = textMaxLen;
		buttonTextTask.center = true;
		
		bool pressed = false;
		const bool collided = _loading ? false : CollidesShapeInt(drawInfo.origin - 
			glm::ivec2(buttonRenderTask.scale.x / 2, 0) * static_cast<int32_t>(drawInfo.center),
			buttonRenderTask.scale + glm::ivec2(0, 9), info.inputState.mousePos);
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

		CardDrawInfo cardDrawInfo{};
		cardDrawInfo.center = true;
		cardDrawInfo.lifeTime = drawInfo.lifeTime;
		
		uint32_t choice = -1;
		if(drawInfo.outStackSelected)
			*drawInfo.outStackSelected = -1;

		uint32_t xOffset = 0;

		if(drawInfo.spawning && drawInfo.length > 1)
		{
			const auto w = GetCardShape(info, drawInfo).x;
			const auto curve = je::CreateCurveOvershooting();
			const float eval = curve.REvaluate(drawInfo.spawnLerp);

			xOffset = (1.f - eval) * w / 2;
		}

		for (uint32_t i = 0; i < drawInfo.length; ++i)
		{
			int32_t xAddOffset = 0;

			if(drawInfo.damagedIndex == i)
			{
				if (fmodf(drawInfo.lifeTime, .2f) < .1f)
					continue;
			}

			if(drawInfo.spawning && i == drawInfo.length - 1)
			{
				const auto curve = je::CreateCurveOvershooting();
				const float eval = curve.REvaluate(drawInfo.spawnLerp);

				xAddOffset = (1.f - eval) * SIMULATED_RESOLUTION.x * (drawInfo.spawnRight * 2 - 1);
			}

			if(drawInfo.dyingIndex != -1)
			{
				const bool fading = drawInfo.dyingLerp < .5f;
				if (drawInfo.dyingIndex == i && !fading)
					continue;
				if (drawInfo.length > 1 && !fading)
				{
					const auto w = GetCardShape(info, drawInfo).x;
					const auto curve = je::CreateCurveOvershooting();
					const float eval = curve.REvaluate((drawInfo.dyingLerp - .5f) * 2);

					float o = eval / 2.f * w;
					if (drawInfo.dyingIndex < i)
						o *= -1;
					xAddOffset += o;
				}
			}

			cardDrawInfo.origin = drawInfo.overridePosIndex == i ? drawInfo.overridePos : GetCardPosition(info, drawInfo, i);
			cardDrawInfo.origin.x += xOffset;
			cardDrawInfo.origin.x += xAddOffset;

			const bool greyedOut = drawInfo.greyedOutArr ? drawInfo.greyedOutArr[i] : false;
			const bool selected = drawInfo.selectedArr ? drawInfo.selectedArr[i] : drawInfo.highlighted == i;

			cardDrawInfo.fgColor = glm::vec4(1);
			cardDrawInfo.bgColor = glm::vec4(0, 0, 0, 1);
			cardDrawInfo.fgColor = greyedOut ? glm::vec4(.1, .1, .1, 1) : cardDrawInfo.fgColor;
			cardDrawInfo.fgColor = selected ? glm::vec4(0, 1, 0, 1) : cardDrawInfo.fgColor;

			if (drawInfo.dyingIndex == i)
			{
				const auto mul = glm::vec4(glm::vec3(1.f - drawInfo.dyingLerp * 2), 1);
				cardDrawInfo.bgColor *= mul;
				cardDrawInfo.fgColor *= mul;
			}

			cardDrawInfo.card = drawInfo.cards[i];
			cardDrawInfo.selectable = true;
			const bool collides = CollidesCard(info, cardDrawInfo);
			uint32_t stackedSelected = -1;
			uint32_t stackedCount = -1;

			cardDrawInfo.ignoreAnim = true;
			cardDrawInfo.hoverDuration = nullptr;
			
			if (drawInfo.stacks)
			{
				stackedCount = drawInfo.stackCounts[i];

				if(!collides)
					for (uint32_t j = 0; j < stackedCount; ++j)
					{
						auto stackedDrawInfo = cardDrawInfo;
						stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (j + 1));
						if(CollidesCard(info, stackedDrawInfo))
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
					stackedDrawInfo.selectable = !collides && stackedSelected == j;
					DrawCard(info, stackedDrawInfo);
				}
			}

			if(drawInfo.combatStats)
				cardDrawInfo.combatStats = &drawInfo.combatStats[i];
			cardDrawInfo.ignoreAnim = stackedSelected != -1;
			if (drawInfo.costs)
				cardDrawInfo.cost = drawInfo.costs[i];
			if(drawInfo.hoverDurations && !stackedSelected != -1)
				cardDrawInfo.hoverDuration = &drawInfo.hoverDurations[i];
			DrawCard(info, cardDrawInfo);
			cardDrawInfo.combatStats = nullptr;
			cardDrawInfo.cost = -1;

			if (stackedSelected != -1)
			{
				auto stackedDrawInfo = cardDrawInfo;
				stackedDrawInfo.card = drawInfo.stacks[i][stackedSelected];
				stackedDrawInfo.origin.y += static_cast<int32_t>(CARD_STACKED_SPACING * (stackedCount - stackedSelected));
				stackedDrawInfo.selectable = true;
				stackedDrawInfo.ignoreAnim = false;
				stackedDrawInfo.hoverDuration = nullptr;
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
				textTask.lifetime = drawInfo.lifeTime;
				textTask.center = true;
				info.textTasks.Push(textTask);
			}

			if ((collides || stackedSelected != -1) && !greyedOut)
				choice = i;
		}

		return choice;
	}

	bool Level::DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo)
	{
		const auto& cardTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::card)];
		jv::ge::SubTexture cardFrames[CARD_FRAME_COUNT];
		Divide(cardTexture.subTexture, cardFrames, CARD_FRAME_COUNT);

		auto origin = drawInfo.origin;
		if (drawInfo.hoverDuration)
		{
			const auto curve = je::CreateCurveOvershooting();
			origin.y += curve.REvaluate(*drawInfo.hoverDuration) * 4;
		}

		PixelPerfectRenderTask bgRenderTask{};
		bgRenderTask.scale = cardTexture.resolution / glm::ivec2(CARD_FRAME_COUNT, 1);
		bgRenderTask.subTexture = cardFrames[0];
		bgRenderTask.xCenter = drawInfo.center;
		bgRenderTask.yCenter = drawInfo.center;

		const bool collided = CollidesShapeInt(drawInfo.origin - 
			(drawInfo.center ? bgRenderTask.scale / 2 : glm::ivec2(0)), bgRenderTask.scale, info.inputState.mousePos);
		bgRenderTask.color = collided && drawInfo.selectable ? glm::vec4(1, 0, 0, 1) : drawInfo.bgColor;
		if(drawInfo.hoverDuration && *drawInfo.hoverDuration > 1e-5f)
		{
			bgRenderTask.color = glm::vec4(*drawInfo.hoverDuration, 0, 0, 1);
		}

		if (drawInfo.hoverDuration)
		{
			*drawInfo.hoverDuration += 5 * info.deltaTime * (collided * 2 - 1);
			*drawInfo.hoverDuration = jv::Clamp(*drawInfo.hoverDuration, 0.f, 1.f);
		}

		bgRenderTask.position = origin;
		info.pixelPerfectRenderTasks.Push(bgRenderTask);

		// Draw image.
		if(!drawInfo.ignoreAnim && drawInfo.card)
		{
			jv::ge::SubTexture animFrames[CARD_MONSTER_ANIM_LENGTH];
			Divide({}, animFrames, CARD_MONSTER_ANIM_LENGTH);

			auto i = static_cast<uint32_t>(GetTime() / CARD_MONSTER_ANIM_SPEED);
			i %= CARD_MONSTER_ANIM_LENGTH;

			PixelPerfectRenderTask imageRenderTask{};
			imageRenderTask.position = origin;
			imageRenderTask.image = info.textureStreamer.Get(drawInfo.card->animIndex);
			imageRenderTask.xCenter = drawInfo.center;
			imageRenderTask.yCenter = drawInfo.center;
			imageRenderTask.subTexture = animFrames[i];
			info.pixelPerfectRenderTasks.Push(imageRenderTask);
		}

		PixelPerfectRenderTask fgRenderTask = bgRenderTask;
		fgRenderTask.subTexture = cardFrames[1];
		fgRenderTask.color = drawInfo.fgColor;
		info.pixelPerfectRenderTasks.Push(fgRenderTask);

		const bool priority = !info.inputState.rMouse.pressed;
		const auto& statsTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::stats)];
		jv::ge::SubTexture statFrames[5];
		Divide(statsTexture.subTexture, statFrames, 5);

		if(drawInfo.cost != -1)
		{
			PixelPerfectRenderTask costRenderTask{};
			costRenderTask.position = origin + glm::ivec2(0, bgRenderTask.scale.y / 2);
			costRenderTask.scale = statsTexture.resolution / glm::ivec2(5, 1);
			costRenderTask.xCenter = drawInfo.center;
			costRenderTask.yCenter = drawInfo.center;
			costRenderTask.color = drawInfo.fgColor;
			costRenderTask.subTexture = statFrames[4];
			costRenderTask.priority = priority;
			info.pixelPerfectRenderTasks.Push(costRenderTask);

			TextTask textTask{};
			textTask.position = costRenderTask.position;
			textTask.center = drawInfo.center;
			textTask.position.y -= 4;
			textTask.text = TextInterpreter::IntToConstCharPtr(drawInfo.cost, info.frameArena);
			textTask.priority = priority;
			info.textTasks.Push(textTask);
		}

		if (drawInfo.combatStats)
		{
			PixelPerfectRenderTask statsRenderTask{};
			statsRenderTask.position = origin + bgRenderTask.scale / 2;
			statsRenderTask.position.x -= 6;
			statsRenderTask.scale = statsTexture.resolution / glm::ivec2(5, 1);
			statsRenderTask.xCenter = drawInfo.center;
			statsRenderTask.yCenter = drawInfo.center;
			statsRenderTask.color = drawInfo.fgColor;
			statsRenderTask.priority = priority;

			uint32_t values[3]
			{
				drawInfo.combatStats->armorClass,
				drawInfo.combatStats->health,
				drawInfo.combatStats->attack
			};

			for (uint32_t i = 0; i < 3; ++i)
			{
				statsRenderTask.subTexture = statFrames[i + 1];
				statsRenderTask.position.y -= statsRenderTask.scale.y;
				info.pixelPerfectRenderTasks.Push(statsRenderTask);

				TextTask textTask{};
				textTask.position = statsRenderTask.position + glm::ivec2(2, -statsRenderTask.scale.y / 2);
				textTask.text = TextInterpreter::IntToConstCharPtr(values[i], info.frameArena);
				textTask.lifetime = drawInfo.lifeTime;
				textTask.priority = priority;
				info.textTasks.Push(textTask);
			}
		}

		if (drawInfo.selectable && collided && info.inputState.rMouse.pressed)
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
		if (_fullCard)
			return;
		_fullCard = card;
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
		headerDrawInfo.text = "press enter to continue...";
		headerDrawInfo.center = true;
		headerDrawInfo.overrideLifeTime = overrideLifeTime;
		headerDrawInfo.scale = 1;
		DrawHeader(info, headerDrawInfo);
	}
	uint32_t Level::DrawParty(const LevelUpdateInfo& info, const PartyDrawInfo& drawInfo)
	{
		const auto& playerState = info.playerState;

		Card* cards[PARTY_CAPACITY]{};
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			cards[i] = &info.monsters[playerState.monsterIds[i]];

		Card** artifacts[PARTY_CAPACITY]{};
		uint32_t artifactCounts[PARTY_CAPACITY]{};

		for (uint32_t i = 0; i < playerState.partySize; ++i)
		{
			const uint32_t count = artifactCounts[i] = playerState.artifactSlotCounts[i];
			if (count == 0)
				continue;
			const auto arr = jv::CreateArray<Card*>(info.frameArena, count);
			artifacts[i] = arr.ptr;
			for (uint32_t j = 0; j < count; ++j)
			{
				const uint32_t index = playerState.artifacts[i * MONSTER_ARTIFACT_CAPACITY + j];
				arr[j] = index == -1 ? nullptr : &info.artifacts[index];
			}
		}

		CombatStats combatInfos[PARTY_CAPACITY];
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			combatInfos[i] = GetCombatStat(info.monsters[playerState.monsterIds[i]]);

		CardSelectionDrawInfo cardSelectionDrawInfo{};
		cardSelectionDrawInfo.cards = cards;
		cardSelectionDrawInfo.length = playerState.partySize;
		cardSelectionDrawInfo.selectedArr = drawInfo.selectedArr;
		cardSelectionDrawInfo.height = drawInfo.height;
		cardSelectionDrawInfo.stacks = artifacts;
		cardSelectionDrawInfo.stackCounts = artifactCounts;
		cardSelectionDrawInfo.lifeTime = _timeSinceOpened;
		cardSelectionDrawInfo.greyedOutArr = drawInfo.greyedOutArr;
		cardSelectionDrawInfo.combatStats = combatInfos;
		cardSelectionDrawInfo.hoverDurations = _hoverDurations;

		return DrawCardSelection(info, cardSelectionDrawInfo);
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
		info.armorClass = card.armorClass;
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

	void Level::Load(const LevelIndex index)
	{
		_loading = true;
		_loadingLevelIndex = index;
		_timeSinceLoading = 0;
	}
}
