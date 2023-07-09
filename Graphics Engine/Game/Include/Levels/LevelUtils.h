#pragma once
#include "JLib/Vector.h"
#include "JLib/Array.h"
#include "States/PlayerState.h"

namespace game
{
	struct Card;
	struct LevelUpdateInfo;

	uint32_t RenderCards(const LevelUpdateInfo& info, Card** cards, uint32_t length, glm::vec2 position, uint32_t highlight = -1);

	bool ValidateMonsterInclusion(uint32_t id, const PlayerState& playerState);
	bool ValidateArtifactInclusion(uint32_t id, const PlayerState& playerState);

	template <typename T>
	void GetDeck(jv::Vector<uint32_t>& outDeck, const jv::Array<T>& cards, const PlayerState& playerState,
		bool(*func)(uint32_t, const PlayerState&))
	{
		outDeck.Clear();
		for (uint32_t i = 0; i < outDeck.length; ++i)
		{
			if (cards[i].unique)
				continue;
			if (!func(i, playerState))
				continue;
			outDeck.Add() = i;
		}
	}
}
