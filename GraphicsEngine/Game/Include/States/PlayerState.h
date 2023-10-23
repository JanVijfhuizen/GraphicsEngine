#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t artifacts[PARTY_CAPACITY * MONSTER_ARTIFACT_CAPACITY];
		uint32_t artifactSlotCount = 1;
		uint32_t partySize = 0;
		bool ironManMode = false;
		
		[[nodiscard]] static PlayerState Create();
	};
}