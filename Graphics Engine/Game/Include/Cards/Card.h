#pragma once

namespace game
{
	struct Card
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "no rule text yet";
		uint32_t count = 1;
	};
}