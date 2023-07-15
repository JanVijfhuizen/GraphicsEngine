#include "pch_game.h"
#include "States/PlayerState.h"

namespace game
{
	void PlayerState::AddMonster(const uint32_t id)
	{
		assert(partySize < PARTY_CAPACITY);
		monsterIds[partySize++] = id;
	}

	void PlayerState::AddArtifact(const uint32_t monster, const uint32_t id)
	{
		assert(artifactSlotCounts[monster] < MONSTER_ARTIFACT_CAPACITY);
		artifacts[monster * MONSTER_ARTIFACT_CAPACITY + artifactSlotCounts[monster]++] = id;
	}

	PlayerState PlayerState::Create()
	{
		PlayerState state{};
		for (auto& artifact : state.artifacts)
			artifact = -1;
		return state;
	}
}
