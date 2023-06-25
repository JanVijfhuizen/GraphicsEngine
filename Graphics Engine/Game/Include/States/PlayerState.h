#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t partySize = 0;
		uint32_t partyIds[MAX_PARTY_SIZE]{};
		uint32_t artifactsIds[MAX_PARTY_SIZE * MAX_ARTIFACTS_PER_CHARACTER]{};
		uint32_t artifactCounts[MAX_PARTY_SIZE]{};
	};
}