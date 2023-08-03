#pragma once

namespace je
{
	struct Curve;
}

namespace game
{
	struct TextTask final
	{
		const char* text = nullptr;
		glm::ivec2 position{};
		uint32_t scale = 1;
		int32_t spacing = 0;
		uint32_t lineLength = 96;
		uint32_t maxLength = UINT32_MAX;
		bool drawDotsOnMaxLengthReached = true;
		bool center = false;
		bool priority = false;
		je::Curve* bounceCurve = nullptr;
		uint32_t bounceMul = 3;
		float lerp = 1;
	};
}
