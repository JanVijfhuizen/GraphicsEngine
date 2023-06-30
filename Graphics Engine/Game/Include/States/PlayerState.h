#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t healths[PARTY_CAPACITY]{};
		uint32_t artifacts[PARTY_CAPACITY * MONSTER_ARTIFACT_CAPACITY];
		uint8_t artifactsCounts[PARTY_CAPACITY]{};
		uint32_t partySize = 0;
	};
}