﻿#pragma once
#include "JLib/Vector.h"
#include "JLib/Array.h"

namespace game
{
	struct GameState;
	
	void RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState);
	void RemoveFlawsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState);
	void RemoveMagicsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState);
	void RemoveDuplicates(jv::Vector<uint32_t>& deck, const uint32_t* duplicates, uint32_t duplicateCount);

	template <typename T>
	void GetDeck(jv::Vector<uint32_t>* outDeck, uint32_t* outCount, const jv::Array<T>& cards)
	{
		if(outDeck)
			outDeck->Clear();

		if (outCount)
			*outCount = 0;

		for (uint32_t i = 0; i < cards.length; ++i)
		{
			const auto& card = cards[i];
			if (card.unique)
				continue;

			if (outCount)
				++*outCount;
			if(outDeck)
				outDeck->Add() = i;
		}
	}
}
