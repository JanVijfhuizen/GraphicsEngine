#include "pch_game.h"
#include "Levels/LevelUtils.h"

#include "Interpreters/TextInterpreter.h"
#include "JLib/Math.h"
#include "Levels/Level.h"
#include "States/GameState.h"
#include "States/InputState.h"
#include "States/PlayerState.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"
#include "Utils/BoxCollision.h"

namespace game
{
	glm::vec2 GetCardPosition(const RenderCardInfo& info, const uint32_t i)
	{
		const float offset = -(CARD_WIDTH_OFFSET + info.additionalSpacing) * (jv::Min<uint32_t>(info.length, info.lineLength) - 1) / 2;
		const uint32_t w = i % info.lineLength;
		const uint32_t h = i / info.lineLength;
		const auto pos = info.position + glm::vec2(
			offset + (CARD_WIDTH_OFFSET + info.additionalSpacing) * static_cast<float>(w),
			h * CARD_HEIGHT * 2);
		return pos;
	}

	RenderMonsterCardReturnInfo RenderMonsterCards(jv::Arena& frameArena, const RenderMonsterCardInfo& info)
	{
		RenderMonsterCardReturnInfo ret{};
		ret.selectedMonster = RenderCards(info);

		TextTask textTask{};
		textTask.center = true;
		textTask.scale = CARD_STAT_SIZE;

		constexpr float f = CARD_HEIGHT * CARD_PIC_FILL_HEIGHT;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto pos = GetCardPosition(info, i);
			const auto monsterCard = static_cast<MonsterCard*>(info.cards[i]);

			textTask.position = pos - glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, -f);
			textTask.text = TextInterpreter::IntToConstCharPtr(monsterCard->attack, frameArena);
			info.levelUpdateInfo->textTasks.Push(textTask);
			textTask.position = pos + glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, f);

			const char* maxHealthText = TextInterpreter::IntToConstCharPtr(monsterCard->health, frameArena);
			textTask.text = maxHealthText;

			if(info.currentHealthArr)
			{
				const uint32_t l1 = GetNumberOfDigits(info.currentHealthArr[i]);
				const uint32_t l2 = GetNumberOfDigits(monsterCard->health);
				const char* currentHealthText = TextInterpreter::IntToConstCharPtr(info.currentHealthArr[i], frameArena);
				const auto ptr = static_cast<char*>(frameArena.Alloc(l1 + l2 + 1));
				memcpy(ptr, currentHealthText, l1);
				memcpy(&ptr[l1 + 1], maxHealthText, l2 + 1);
				ptr[l1] = '/';
				textTask.text = ptr;
			}

			if(info.flawArr && info.flawArr[i])
			{
				auto flawPos = pos - glm::vec2(0, CARD_HEIGHT + CARD_STACKED_TOP_SIZE);
				constexpr auto scale = glm::vec2(CARD_WIDTH, CARD_STACKED_TOP_SIZE);
				const auto mousePos = info.levelUpdateInfo->inputState.mousePos;
				const bool collided = CollidesShape(flawPos, scale, mousePos);

				if (collided)
				{
					flawPos.y -= CARD_SELECTED_Y_POSITION_INCREASE;

					Card* flaw[]{ info.flawArr[i] };
					RenderCardInfo renderFlawCardInfo{};
					renderFlawCardInfo.levelUpdateInfo = info.levelUpdateInfo;
					renderFlawCardInfo.position = LARGE_CARD_POS;
					renderFlawCardInfo.cards = flaw;
					renderFlawCardInfo.scale *= CARD_LARGE_SIZE_INCREASE_MUL;
					renderFlawCardInfo.priority = true;
					renderFlawCardInfo.interactable = false;
					RenderCards(renderFlawCardInfo);

					ret.selectedMonster = i;
					ret.selectedFlaw = i;
				}

				RenderTask stackedRenderTask{};
				stackedRenderTask.scale = scale;
				stackedRenderTask.position = flawPos;
				stackedRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
				stackedRenderTask.color *= collided ? 1 : CARD_DARKENED_COLOR_MUL;
				info.levelUpdateInfo->renderTasks.Push(stackedRenderTask);

				TextTask titleTextTask{};
				titleTextTask.lineLength = 12;
				titleTextTask.center = true;
				titleTextTask.position = flawPos;
				titleTextTask.text = info.flawArr[i]->name;
				titleTextTask.scale = CARD_TITLE_SIZE;
				info.levelUpdateInfo->textTasks.Push(titleTextTask);
			}

			if(info.artifactArr && info.artifactArr[i])
			{
				const uint32_t startIndex = info.flawArr && info.flawArr[i] ? 1 : 0;
				const uint32_t artifactCount = info.artifactCounts[i];
				for (uint32_t j = 0; j < artifactCount; ++j)
				{
					auto artifactPos = pos - glm::vec2(0, CARD_HEIGHT + CARD_STACKED_TOP_SIZE * static_cast<float>(j + 1 + startIndex));
					constexpr auto scale = glm::vec2(CARD_WIDTH, CARD_STACKED_TOP_SIZE);

					const auto mousePos = info.levelUpdateInfo->inputState.mousePos;
					const bool collided = CollidesShape(artifactPos, scale, mousePos);
					if (collided)
					{
						artifactPos.y -= CARD_SELECTED_Y_POSITION_INCREASE;

						Card* flaw[]{ info.artifactArr[i][j] };
						RenderCardInfo renderFlawCardInfo{};
						renderFlawCardInfo.levelUpdateInfo = info.levelUpdateInfo;
						renderFlawCardInfo.position = LARGE_CARD_POS;
						renderFlawCardInfo.cards = flaw;
						renderFlawCardInfo.scale *= CARD_LARGE_SIZE_INCREASE_MUL;
						renderFlawCardInfo.priority = true;
						renderFlawCardInfo.interactable = false;
						RenderCards(renderFlawCardInfo);

						ret.selectedArtifact = j;
						ret.selectedMonster = i;
					}

					RenderTask stackedRenderTask{};
					stackedRenderTask.scale = scale;
					stackedRenderTask.position = artifactPos;
					stackedRenderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::fallback)];
					stackedRenderTask.color *= collided ? 1 : CARD_DARKENED_COLOR_MUL;
					info.levelUpdateInfo->renderTasks.Push(stackedRenderTask);

					if(const auto artifactCard = info.artifactArr[i][j])
					{
						TextTask titleTextTask{};
						titleTextTask.lineLength = 12;
						titleTextTask.center = true;
						titleTextTask.position = artifactPos;
						titleTextTask.text = artifactCard->name;
						titleTextTask.scale = CARD_TITLE_SIZE;
						info.levelUpdateInfo->textTasks.Push(titleTextTask);
					}
				}
			}

			info.levelUpdateInfo->textTasks.Push(textTask);
		}

		return ret;
	}

	uint32_t RenderMagicCards(jv::Arena& frameArena, const RenderCardInfo& info)
	{
		const auto result = RenderCards(info);

		TextTask costTextTask{};
		costTextTask.center = true;
		costTextTask.scale = CARD_STAT_SIZE;

		for (uint32_t i = 0; i < info.length; ++i)
		{
			const auto pos = GetCardPosition(info, i);
			const auto magicCard = static_cast<MagicCard*>(info.cards[i]);
			costTextTask.position = pos - glm::vec2(CARD_WIDTH - CARD_BORDER_OFFSET, CARD_HEIGHT);
			costTextTask.text = TextInterpreter::IntToConstCharPtr(magicCard->cost, frameArena);
			info.levelUpdateInfo->textTasks.Push(costTextTask);
		}

		return result;
	}

	uint32_t RenderCards(const RenderCardInfo& info)
	{
		uint32_t collided = -1;
		const auto mousePos = info.levelUpdateInfo->inputState.mousePos;

		RenderTask renderTask{};
		renderTask.scale *= info.scale;
		renderTask.subTexture = info.levelUpdateInfo->subTextures[static_cast<uint32_t>(TextureId::card)];

		for (uint32_t i = 0; i < info.length; ++i)
		{
			auto pos = GetCardPosition(info, i);
			const auto card = info.cards[i];

			if (info.interactable && CollidesShape(pos, glm::vec2(CARD_WIDTH, CARD_HEIGHT), mousePos))
			{
				collided = i;
				pos.y -= CARD_SELECTED_Y_POSITION_INCREASE;

				RenderCardInfo renderLargeCardInfo{};
				renderLargeCardInfo.levelUpdateInfo = info.levelUpdateInfo;
				renderLargeCardInfo.position = LARGE_CARD_POS;
				renderLargeCardInfo.cards = &info.cards[i];
				renderLargeCardInfo.scale *= CARD_LARGE_SIZE_INCREASE_MUL;
				renderLargeCardInfo.priority = true;
				renderLargeCardInfo.interactable = false;
				RenderCards(renderLargeCardInfo);
			}

			renderTask.position = pos;
			renderTask.color = glm::vec4(1);
			if (info.selectedArr && !info.selectedArr[i])
				renderTask.color *= CARD_DARKENED_COLOR_MUL;
			if (info.highlight != -1 && info.highlight != i)
				renderTask.color *= CARD_DARKENED_COLOR_MUL;

			if (info.priority)
				info.levelUpdateInfo->priorityRenderTasks.Push(renderTask);
			else
				info.levelUpdateInfo->renderTasks.Push(renderTask);

			TextTask titleTextTask{};
			titleTextTask.lineLength = 12;
			titleTextTask.center = true;
			titleTextTask.position = pos - glm::vec2(0, renderTask.scale.y);
			titleTextTask.text = card->name;
			titleTextTask.scale = CARD_TITLE_SIZE * CARD_LARGE_SIZE_INCREASE_MUL;
			info.levelUpdateInfo->priorityTextTasks.Push(titleTextTask);

			TextTask ruleTextTask = titleTextTask;
			ruleTextTask.position = pos + glm::vec2(0, renderTask.scale.y);
			ruleTextTask.text = card->ruleText;
			ruleTextTask.scale = CARD_TEXT_SIZE * CARD_LARGE_SIZE_INCREASE_MUL;
			info.levelUpdateInfo->priorityTextTasks.Push(ruleTextTask);
		}

		return collided;
	}

	void RemoveMonstersInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		RemoveDuplicates(deck, playerState.monsterIds, playerState.partySize);
	}

	void RemoveArtifactsInParty(jv::Vector<uint32_t>& deck, const PlayerState& playerState)
	{
		for (uint32_t i = 0; i < playerState.partySize; ++i)
		{
			for (uint32_t j = 0; j < MONSTER_ARTIFACT_CAPACITY; ++j)
				for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
					if (playerState.artifacts[MONSTER_ARTIFACT_CAPACITY * i + j] == deck[k])
					{
						deck.RemoveAt(k);
						break;
					}
		}
	}

	void RemoveFlawsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (uint32_t i = 0; i < gameState.partySize; ++i)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if (gameState.flaws[k] == deck[k])
				{
					deck.RemoveAt(k);
					break;
				}
	}

	void RemoveMagicsInParty(jv::Vector<uint32_t>& deck, const GameState& gameState)
	{
		for (auto& magic : gameState.magics)
			for (int32_t k = static_cast<int32_t>(deck.count) - 1; k >= 0; --k)
				if(deck[k] == magic)
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
