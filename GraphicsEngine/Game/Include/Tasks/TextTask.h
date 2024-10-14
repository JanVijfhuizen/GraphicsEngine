#pragma once

namespace game
{
	struct TextTask final
	{
		const char* text = nullptr;
		glm::ivec2 position{};
		glm::vec4 color{1};
		int32_t spacing = 0;
		uint32_t lineLength = 96;
		uint32_t maxLength = -1;
		bool xCenter = false;
		bool yCenter = false;
		bool priority = false;
		bool front = false;

		float lifetime = -1;
		bool loop = false;
		bool fadeIn = true;
		bool largeFont = false;

		bool textBubble = false;
		bool textBubbleTail = false;
		bool inverseTail = false;
		uint32_t scale = 1;
	};
}
