#pragma once

namespace game
{
	struct State;

	struct ActionState final
	{
		struct VDamage
		{
			enum
			{
				damage
			};
		};

		struct VStatBuff
		{
			enum
			{
				attack,
				health,
				tempAttack,
				tempHealth
			};
		};

		struct VStatSet
		{
			enum
			{
				attack,
				health,
				tempAttack,
				tempHealth
			};
		};

		struct VSummon
		{
			enum
			{
				isAlly,
				id,
				attack,
				health,
				untapped,
			};
		};

		enum class Trigger
		{
			onDraw,
			onSummon,
			onAttack,
			onDamage,
			onStatBuff,
			onStatSet,
			onDeath,
			onCast,
			onStartOfTurn,
			onEndOfTurn
		} trigger;

		enum class Source
		{
			board,
			other
		} source = Source::board;

		uint32_t src = UINT32_MAX;
		uint32_t dst = UINT32_MAX;
		uint32_t srcUniqueId = UINT32_MAX;
		uint32_t dstUniqueId = UINT32_MAX;
		uint32_t values[6]
		{
			UINT32_MAX, UINT32_MAX,
			UINT32_MAX, UINT32_MAX,
			UINT32_MAX, UINT32_MAX
		};
	};

	struct Card
	{
		enum class Type
		{
			artifact,
			curse,
			event,
			monster,
			room,
			spell
		};

		bool unique = false;
		const char* name = "unnamed";
		const char* ruleText = "...";
		uint32_t animIndex = 0;
		uint32_t normalAnimIndex = -1;
		uint32_t animFrameCount = 1;

		bool(*onActionEvent)(const struct LevelInfo& info, State& state, const ActionState& actionState, uint32_t self) = nullptr;
		[[nodiscard]] virtual Type GetType() = 0;
	};
}
