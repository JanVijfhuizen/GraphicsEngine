#pragma once

namespace game
{
	struct MonsterCard final
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "";
		const char* flavorText = "this one does not have flavor";
	};
}