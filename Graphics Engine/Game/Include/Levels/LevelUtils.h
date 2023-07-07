#pragma once

namespace game
{
	struct Card;
	struct LevelUpdateInfo;

	uint32_t RenderCards(const LevelUpdateInfo& info, Card** cards, uint32_t length, glm::vec2 position, uint32_t highlight = -1);
}
