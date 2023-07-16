﻿#include "pch_game.h"
#include "Levels/LevelUtils.h"

#include "Interpreters/TextInterpreter.h"
#include "JLib/Math.h"
#include "Levels/Level.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"
#include "Utils/BoxCollision.h"

namespace game
{
	glm::vec2 GetCardPosition(const RenderCardInfo& info, const uint32_t i)
	{
		const float offset = -(CARD_WIDTH_OFFSET + info.additionalSpacing) * (jv::Min<uint32_t>(info.length, info.lineLength) - 1) / 2;
		const uint32_t w = i % info.lineLength;
		const uint32_t h = i / info.lineLength;
		const auto pos = info.center + glm::vec2(
			offset + (CARD_WIDTH_OFFSET + info.additionalSpacing) * static_cast<float>(w),
			h * CARD_HEIGHT * 2);
		return pos;
	}

	uint32_t RenderMonsterCards(jv::Arena& frameArena, const RenderCardInfo& info)
	{
		const auto result = RenderCards(info);

		TextTask textTask{};
		textTask.center = true;
		textTask.scale = CARD_STAT_SIZE;

		constexpr float f = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto pos = GetCardPosition(info, i);
			const auto monsterCard = static_cast<MonsterCard*>(info.cards[i]);
			textTask.position = pos - glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, -f);
			textTask.text = TextInterpreter::IntToConstCharPtr(monsterCard->attack, frameArena);
			info.levelUpdateInfo->textTasks.Push(textTask);
			textTask.position = pos + glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, f);
			textTask.text = TextInterpreter::IntToConstCharPtr(monsterCard->health, frameArena);
			info.levelUpdateInfo->textTasks.Push(textTask);
		}

		return result;
	}

	uint32_t RenderMagicCards(jv::Arena& frameArena, const RenderCardInfo& info)
	{
		const auto result = RenderCards(info);

		TextTask costTextTask{};
		costTextTask.center = true;
		costTextTask.scale = CARD_STAT_SIZE;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto pos = GetCardPosition(info, i);
			const auto magicCard = static_cast<MagicCard*>(info.cards[i]);
			costTextTask.position = pos - glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, CARD_HEIGHT);
			costTextTask.text = TextInterpreter::IntToConstCharPtr(magicCard->cost, frameArena);
			info.levelUpdateInfo->textTasks.Push(costTextTask);
		}

		return result;
	}

	uint32_t RenderCards(const RenderCardInfo& info)
	{
		uint32_t selected = -1;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto card = info.cards[i];
			auto pos = GetCardPosition(info, i);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), info.levelUpdateInfo->inputState.mousePos))
			{
				selected = i;
				pos.y -= CARD_SELECTED_Y_POSITION_INCREASE;

				// Draw large version.
				if(card)
				{
					RenderTask bgRenderTask{};
					bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
					bgRenderTask.scale.x = CARD_WIDTH;
					bgRenderTask.position = pos + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
					bgRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
					info.levelUpdateInfo->renderTasks.Push(bgRenderTask);

					RenderTask picRenderTask{};
					picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
					picRenderTask.scale.x = CARD_WIDTH;
					picRenderTask.position = pos - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
					picRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
					info.levelUpdateInfo->renderTasks.Push(picRenderTask);

					TextTask titleTextTask{};
					titleTextTask.lineLength = 12;
					titleTextTask.center = true;
					titleTextTask.position = pos - glm::vec2(0, CARD_HEIGHT);
					titleTextTask.text = card->name;
					titleTextTask.scale = CARD_TITLE_SIZE;
					info.levelUpdateInfo->textTasks.Push(titleTextTask);

					TextTask ruleTextTask = titleTextTask;
					ruleTextTask.position = pos + glm::vec2(0, bgRenderTask.scale.y / 2);
					ruleTextTask.text = card->ruleText;
					ruleTextTask.maxLength = CARD_SMALL_TEXT_CAPACITY;
					ruleTextTask.scale = CARD_TEXT_SIZE;
					info.levelUpdateInfo->textTasks.Push(ruleTextTask);
				}
			}

			auto finalColor = glm::vec4(1);
			finalColor *= info.highlight != -1 ? info.highlight == i ? 1 : CARD_DARKENED_COLOR_MUL : 1;
			finalColor *= info.selectedArr ? info.selectedArr[i] ? 1 : CARD_DARKENED_COLOR_MUL : 1;

			RenderTask bgRenderTask{};
			bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
			bgRenderTask.scale.x = CARD_WIDTH;
			bgRenderTask.position = pos + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
			bgRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
			bgRenderTask.color = finalColor;
			info.levelUpdateInfo->renderTasks.Push(bgRenderTask);

			RenderTask picRenderTask{};
			picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
			picRenderTask.scale.x = CARD_WIDTH;
			picRenderTask.position = pos - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
			picRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
			picRenderTask.color = finalColor;
			info.levelUpdateInfo->renderTasks.Push(picRenderTask);

			if(card)
			{
				TextTask titleTextTask{};
				titleTextTask.lineLength = 12;
				titleTextTask.center = true;
				titleTextTask.position = pos - glm::vec2(0, CARD_HEIGHT);
				titleTextTask.text = card->name;
				titleTextTask.scale = CARD_TITLE_SIZE;
				info.levelUpdateInfo->textTasks.Push(titleTextTask);

				TextTask ruleTextTask = titleTextTask;
				ruleTextTask.position = pos + glm::vec2(0, bgRenderTask.scale.y / 2);
				ruleTextTask.text = card->ruleText;
				ruleTextTask.maxLength = CARD_SMALL_TEXT_CAPACITY;
				ruleTextTask.scale = CARD_TEXT_SIZE;
				info.levelUpdateInfo->textTasks.Push(ruleTextTask);
			}
		}

		return selected;
	}

	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		RemoveDuplicates(deck, playerState.monsterIds, playerState.partySize);
	}

	void RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		for (uint32_t i = 0; i < playerState.partySize; ++i)
		{
			for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
				for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
					if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * i + j] == deck[k])
					{
						deck.RemoveAt(k);
						break;
					}
		}
	}

	void RemoveFlawsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (uint32_t i = 0; i < gameState.partySize; ++i)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if (gameState.flaws[k] == deck[k])
				{
					deck.RemoveAt(k);
					break;
				}
	}

	void RemoveMagicsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (auto& magic : gameState.magics)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if(deck[k] == magic)
				{
					deck.RemoveAt(k);
					break;
				}
	}

	void RemoveDuplicates(jv::Vector<uint32_t>& deck, const uint32_t* duplicates, const uint32_t duplicateCount)
	{
		for (uint32_t i = 0; i < duplicateCount; ++i)
			for (uint32_t j = 0; j < deck.count; ++j)
				if(deck[j] == duplicates[i])
				{
					deck.RemoveAt(j);
					break;
				}
	}

	bool ValidateMonsterInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
			if (playerState.monsterIds[j] == id)
				return false;

		return true;
	}

	bool ValidateArtifactInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
		{
			for (uint32_t k = 0; k < MONSTER_ARTIFACT_CAPACITY; ++k)
				if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					return false;
		}
		return true;
	}
}
