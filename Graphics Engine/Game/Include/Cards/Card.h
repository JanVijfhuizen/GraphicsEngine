#pragma once

namespace game
{
	struct Card
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "";
		const char* flavorText = "this one does not have flavor";
		uint32_t count = 1;
	};
}