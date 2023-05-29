#pragma once
#include "Card.h"

namespace game
{
	struct ArtifactCard final : Card
	{
		uint32_t tier;
		bool unique = false;
	};
}
