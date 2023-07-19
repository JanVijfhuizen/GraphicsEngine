#pragma once
#include "Cards/ArtifactCard.h"
#include "JLib/Vector.h"
#include "JLib/Array.h"

namespace game
{
	struct FlawCard;
	struct LevelInfo;
	struct PlayerState;
	struct GameState;
	struct Card;
	struct LevelUpdateInfo;

	struct RenderCardInfo
	{
		LevelUpdateInfo const* levelUpdateInfo;
		Card** cards;
		uint32_t length;
		glm::vec2 center{};
		uint32_t highlight = -1;
		bool* selectedArr = nullptr;
		float additionalSpacing = 0;
		uint32_t lineLength = -1;
	};

	struct RenderMonsterCardInfo final : RenderCardInfo
	{
		uint32_t* currentHealthArr = nullptr;
		ArtifactCard*** artifactArr = nullptr;
		uint32_t* artifactCounts = nullptr;
		FlawCard** flawArr = nullptr;
	};

	struct RenderMonsterCardReturnInfo final
	{
		uint32_t selectedMonster = -1;
		uint32_t selectedArtifact = -1;
		uint32_t selectedFlaw = -1;
	};
	
	RenderMonsterCardReturnInfo RenderMonsterCards(jv::Arena& frameArena, const RenderMonsterCardInfo& info);
	uint32_t RenderMagicCards(jv::Arena& frameArena, const RenderCardInfo& info);
	uint32_t RenderCards(const RenderCardInfo& info);
	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState);
	void RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState);
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

			if(outCount)
				*outCount += card.count;
			if(outDeck)
				for (uint32_t j = 0; j < card.count; ++j)
					outDeck->Add() = i;
		}
	}
}
