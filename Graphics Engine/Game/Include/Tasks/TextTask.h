#pragma once

namespace game
{
	struct TextTask final
	{
		const char* text = nullptr;
		glm::vec2 position{};
		float scale = .1f;
		int32_t spacing = 4;
		uint32_t lineLength = 96;
		int32_t lineSpacing = 0;
		bool center = false;
	};
}