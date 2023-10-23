#pragma once
#include "Card.h"

namespace game
{
	struct ArtifactCard final : Card
	{
		[[nodiscard]] Type GetType() override
		{
			return Type::artifact;
		}
	};
}
