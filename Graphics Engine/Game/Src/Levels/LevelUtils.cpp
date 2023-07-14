#include "pch_game.h"
#include "Levels/LevelUtils.h"

#include "JLib/Math.h"
#include "Levels/Level.h"
#include "States/InputState.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"
#include "Utils/BoxCollision.h"

namespace game
{
	uint32_t RenderCards(const RenderCardInfo& info)
	{
		const float offset = -(CARD_WIDTH_OFFSET + info.additionalSpacing) * (jv::Min<uint32_t>(info.length, info.lineLength) - 1) / 2;
		uint32_t selected = -1;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const uint32_t w = i % info.lineLength;
			const uint32_t h = i / info.lineLength;

			const auto card = info.cards[i];
			const auto pos = info.center + glm::vec2(offset + (CARD_WIDTH_OFFSET + info.additionalSpacing) * static_cast<float>(w), h * CARD_HEIGHT * 2);

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
				titleTextTask.scale = CARD_TEXT_SIZE;
				info.levelUpdateInfo->textTasks.Push(titleTextTask);

				TextTask ruleTextTask = titleTextTask;
				ruleTextTask.position = pos + glm::vec2(0, bgRenderTask.scale.y);
				ruleTextTask.text = card->ruleText;
				info.levelUpdateInfo->textTasks.Push(ruleTextTask);
			}

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), info.levelUpdateInfo->inputState.mousePos))
				selected = i;
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
