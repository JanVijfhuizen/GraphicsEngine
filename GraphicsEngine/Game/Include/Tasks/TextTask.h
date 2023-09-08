#pragma once

namespace game
{
	struct TextTask final
	{
		const char* text = nullptr;
		glm::ivec2 position{};
		glm::vec4 color{1};
		uint32_t scale = 1;
		int32_t spacing = 0;
		uint32_t lineLength = 96;
		uint32_t maxLength = -1;
		bool center = false;
		bool priority = false;

		float lifetime = -1;
		bool loop = false;
		bool fadingOut = false;
	};
}
