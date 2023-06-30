#pragma once

namespace game
{
	struct TextTask final
	{
		const char* text = nullptr;
		glm::vec2 position{};
		float scale = .1f;
		int32_t spacing = 2;
		uint32_t lineLength = 96;
		int32_t lineSpacing = 6;
		bool center = false;
	};
}