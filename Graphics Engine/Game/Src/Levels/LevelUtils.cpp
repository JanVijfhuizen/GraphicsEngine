﻿#include "pch_game.h"
#include "Levels/LevelUtils.h"

#include "Levels/Level.h"
#include "States/InputState.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"
#include "Utils/BoxCollision.h"

namespace game
{
	uint32_t RenderCards(const LevelUpdateInfo& info, Card** cards, uint32_t length, glm::vec2 position, uint32_t highlight)
	{
		const float offset = -CARD_WIDTH_OFFSET * (length - 1) / 2;
		uint32_t selected = -1;
		const auto color = glm::vec4(1) * (highlight < length ? CARD_DARKENED_COLOR_MUL : 1);

		for (uint32_t i = 0; i < length; ++i)
		{
			const auto card = cards[i];
			const auto pos = position + glm::vec2(offset + CARD_WIDTH_OFFSET * static_cast<float>(i), 0);
			const auto finalColor = highlight == i ? glm::vec4(1) : color;

			RenderTask bgRenderTask{};
			bgRenderTask.scale.y = CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT);
			bgRenderTask.scale.x = CARD_WIDTH;
			bgRenderTask.position = pos + glm::vec2(0, CARD_HEIGHT * (1.f - CARD_PIC_FILL_HEIGHT));
			bgRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
			bgRenderTask.color = finalColor;
			info.renderTasks.Push(bgRenderTask);

			RenderTask picRenderTask{};
			picRenderTask.scale.y = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;
			picRenderTask.scale.x = CARD_WIDTH;
			picRenderTask.position = pos - glm::vec2(0, CARD_HEIGHT * CARD_PIC_FILL_HEIGHT);
			picRenderTask.subTexture = info.subTextures[static_cast<uint32_t>(TextureId::fallback)];
			picRenderTask.color = finalColor;
			info.renderTasks.Push(picRenderTask);

			TextTask titleTextTask{};
			titleTextTask.lineLength = 12;
			titleTextTask.center = true;
			titleTextTask.position = pos - glm::vec2(0, CARD_HEIGHT);
			titleTextTask.text = card->name;
			titleTextTask.scale = CARD_TEXT_SIZE;
			info.textTasks.Push(titleTextTask);

			TextTask ruleTextTask = titleTextTask;
			ruleTextTask.position = pos + glm::vec2(0, bgRenderTask.scale.y);
			ruleTextTask.text = card->ruleText;
			info.textTasks.Push(ruleTextTask);

			if (CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), info.inputState.mousePos))
				selected = i;
		}

		return selected;
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
