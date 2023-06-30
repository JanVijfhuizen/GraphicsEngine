#pragma once

namespace game
{
	bool CollidesShape(glm::vec2 center, glm::vec2 shape, glm::vec2 pos);
	bool Collides(glm::vec2 lTop, glm::vec2 rBot, glm::vec2 pos);
}