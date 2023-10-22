#include "pch_game.h"
#include "Levels/LevelUtils.h"
#include "Levels/Level.h"
#include "States/GameState.h"
#include "States/PlayerState.h"

namespace game
{
	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		RemoveDuplicates(deck, playerState.monsterIds, playerState.partySize);
	}

	void RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState, const GameState& gameState)
	{
		for (uint32_t i = 0; i < playerState.partySize; ++i)
			for (uint32_t j = 0; j < playerState.artifactSlotCount; ++j)
				for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
					if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * i + j] == deck[k])
					{
						deck.RemoveAt(k);
						break;
					}
		for (uint32_t i = 0; i < gameState.partySize; ++i)
			for (uint32_t j = 0; j < gameState.artifactSlotCount; ++j)
				for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
					if (gameState.artifacts[MONSTER_ARTIFACT_CAPACITY * i + j] == deck[k])
					{
						deck.RemoveAt(k);
						break;
					}
	}

	void RemoveFlawsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (uint32_t i = 0; i < gameState.partySize; ++i)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if (gameState.curses[k] == deck[k])
				{
					deck.RemoveAt(k);
					break;
				}
	}

	void RemoveMagicsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (auto& magic : gameState.spells)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if (deck[k] == magic)
				{
					deck.RemoveAt(k);
					break;
				}
	}

	void RemoveDuplicates(jv::Vector<uint32_t>& deck, const uint32_t* duplicates, const uint32_t duplicateCount)
	{
		for (uint32_t i = 0; i < duplicateCount; ++i)
			for (uint32_t j = 0; j < deck.count; ++j)
				if(deck[j] == duplicates[i])
				{
					deck.RemoveAt(j);
					break;
				}
	}

	bool ValidateMonsterInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
			if (playerState.monsterIds[j] == id)
				return false;

		return true;
	}

	bool ValidateArtifactInclusion(const uint32_t id, const PlayerState& playerState)
	{
		for (uint32_t j = 0; j < playerState.partySize; ++j)
		{
			for (uint32_t k = 0; k < MONSTER_ARTIFACT_CAPACITY; ++k)
				if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * j + k] == id)
					return false;
		}
		return true;
	}
}
