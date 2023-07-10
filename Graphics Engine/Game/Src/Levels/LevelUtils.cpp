#include "pch_game.h"
#include "Levels/LevelUtils.h"

#include "Levels/Level.h"
#include "States/InputState.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"
#include "Utils/BoxCollision.h"

namespace game
{
	uint32_t RenderCards(const RenderCardInfo& info)
	{
		const float offset = -(CARD_WIDTH_OFFSET + info.additionalSpacing) * (info.length - 1) / 2;
		uint32_t selected = -1;
		const auto color = glm::vec4(1) * (info.highlight < info.length ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto card = info.cards[i];
			const auto pos = info.center + glm::vec2(offset + (CARD_WIDTH_OFFSET + info.additionalSpacing) * static_cast<float>(i), 0);
			const auto finalColor = info.highlight == i ? glm::vec4(1) : color;

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

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), info.levelUpdateInfo->inputState.mousePos))
				selected = i;
		}

		return selected;
	}

	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			for (int32_t j = static_cast<int32_t>(deck.count) - 1; j >= 0; --j)
				if (playerState.monsterIds[i] == deck[j])
				{
					deck.RemoveAt(j);
					break;
				}
	}

	bool RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		for (uint32_t i = 0; i < playerState.partySize; ++i)
		{
			const uint32_t artifactCount = playerState.artifactsCounts[i];
			for (uint32_t j = 0; j < artifactCount; ++j)
				for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
					if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * i + j] == deck[k])
					{
						deck.RemoveAt(k);
						break;
					}
		}
		return true;
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
			const uint32_t artifactCount = playerState.artifactsCounts[j];
			for (uint32_t k = 0; k < artifactCount; ++k)
				if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					return false;
		}
		return true;
	}
}
