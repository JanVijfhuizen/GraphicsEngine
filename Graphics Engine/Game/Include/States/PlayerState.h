#pragma once

namespace game
{
	struct PlayerState final
	{
		uint32_t monsterIds[PARTY_CAPACITY]{};
		uint32_t artifacts[PARTY_CAPACITY * MONSTER_ARTIFACT_CAPACITY];
		uint32_t artifactSlotCounts[PARTY_CAPACITY]{};
		uint32_t partySize = 0;
		bool ironManMode = false;

		void AddMonster(uint32_t id);
		void AddArtifact(uint32_t monster, uint32_t id);

		static PlayerState Create();
	};
}