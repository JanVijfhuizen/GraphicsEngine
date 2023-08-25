#pragma once

namespace game
{
	struct State;

	struct Card
	{
		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "no rule text yet";
		uint32_t count = 1;
		uint32_t animIndex = 0;

		void(*startOfCombat)(State& state, uint32_t self) = nullptr;
		void(*startOfTurn)(State& state, uint32_t self) = nullptr;
		void(*onAttack)(State& state, uint32_t self, uint32_t src, uint32_t dst, uint32_t& roll, uint32_t& damage) = nullptr;
		void(*onDeath)(State& state, uint32_t self, uint32_t src, uint32_t dst) = nullptr;
		void(*onSpellPlayed)(State& state, uint32_t self, uint32_t src, uint32_t dst) = nullptr;
	};
}
