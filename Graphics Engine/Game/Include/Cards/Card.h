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
			onDamage,
			onDeath,
			onCardPlayed,
			onStartOfTurn
		} trigger;
		enum class Source
		{
			board,
			other
		} source = Source::board;

		uint32_t src = -1;
		uint32_t dst = -1;
		uint32_t srcUniqueId = -1;
		uint32_t dstUniqueId = -1;
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
		uint32_t count = 1;
		uint32_t animIndex = 0;

		bool(*onActionEvent)(State& state, ActionState& actionState, uint32_t self) = nullptr;
	};
}
