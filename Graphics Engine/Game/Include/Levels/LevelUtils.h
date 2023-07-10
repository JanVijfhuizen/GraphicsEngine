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
	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState);
	bool RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState);

	template <typename T>
	void GetDeck(jv::Vector<uint32_t>* outDeck, uint32_t* outCount, const jv::Array<T>& cards, const PlayerState& playerState)
	{
		if (outCount)
			*outCount = 0;

		for (uint32_t i = 0; i < cards.length; ++i)
		{
			const auto& card = cards[i];
			if (card.unique)
				continue;

			if(outCount)
				*outCount += card.count;
			if(outDeck)
				for (uint32_t j = 0; j < card.count; ++j)
					outDeck->Add() = i;
		}
	}
}
