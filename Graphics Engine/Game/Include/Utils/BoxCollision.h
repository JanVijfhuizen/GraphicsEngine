#pragma once

namespace game
{
	[[nodiscard]] bool CollidesShape(glm::vec2 center, glm::vec2 shape, glm::vec2 pos);
	[[nodiscard]] bool CollidesShapeInt(glm::ivec2 position, glm::ivec2 scale, glm::ivec2 pos);
	[[nodiscard]] bool Collides(glm::vec2 lTop, glm::vec2 rBot, glm::vec2 pos);
}