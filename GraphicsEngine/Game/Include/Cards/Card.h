#pragma once

namespace game
{
	struct State;

	struct ActionState final
	{
		enum class VDamage
		{
			damage
		};

		enum class VSummon
		{
			isAlly,
			id,
			partyId,
			health
		};

		enum class Trigger
		{
			draw,
			onSummon,
			onAttack,
			onMiss,
			onDamage,
			onDeath,
			onCardPlayed,
			onStartOfTurn
		} trigger;
		enum class Source
		{
			board,
			hand
		} source = Source::board;

		uint32_t src = UINT32_MAX;
		uint32_t dst = UINT32_MAX;
		uint32_t srcUniqueId = UINT32_MAX;
		uint32_t dstUniqueId = UINT32_MAX;
		uint32_t values[4]
		{
			UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
		};
	};

	struct Card
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "no rule text yet";
		uint32_t animIndex = 0;
		uint32_t normalAnimIndex = -1;

		bool(*onActionEvent)(State& state, ActionState& actionState, uint32_t self, bool& actionPending) = nullptr;
	};
}
