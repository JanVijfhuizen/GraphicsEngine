#pragma once
#include "JLib/Vector.h"
#include "JLib/Array.h"
#include "States/PlayerState.h"

namespace game
{
	struct Card;
	struct LevelUpdateInfo;

	struct RenderCardInfo final
	{
		LevelUpdateInfo const* levelUpdateInfo;
		Card** cards;
		uint32_t length;
		glm::vec2 center{};
		uint32_t highlight = -1;
		float additionalSpacing = 0;
	};

	uint32_t RenderCards(const RenderCardInfo& info);

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
