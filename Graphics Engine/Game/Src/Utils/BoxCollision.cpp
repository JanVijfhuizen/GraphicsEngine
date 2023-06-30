#include "pch_game.h"
#include "Utils/BoxCollision.h"

namespace game
{
	bool CollidesShape(const glm::vec2 center, const glm::vec2 shape, const glm::vec2 pos)
	{
		return Collides(center + shape * glm::vec2(-1, 1), center + shape * glm::vec2(1, -1), pos);
	}

	bool Collides(const glm::vec2 lTop, const glm::vec2 rBot, const glm::vec2 pos)
	{
		return pos.x > lTop.x && pos.x < rBot.x && pos.y < lTop.y && pos.y > rBot.y;
	}
}
