#pragma once

namespace game
{
	struct State;

	struct ActionState final
	{
		enum Trigger
		{
			onAttack,
			onDamage,
			onDeath,
			onCardPlayed,
			onStartOfTurn,
			onStartOfRoom
		} trigger;
		enum Source
		{
			board,
			other
		} source = board;
		uint32_t src = -1;
		uint32_t dst = -1;
		uint32_t srcUniqueId;
		uint32_t dstUniqueId;
	};

	struct Card
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "no rule text yet";
		uint32_t count = 1;
		uint32_t animIndex = 0;

		void(*onActionEvent)(State& state, ActionState& actionState, uint32_t self) = nullptr;
	};
}
