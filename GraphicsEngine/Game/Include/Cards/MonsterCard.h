#pragma once
#include "Card.h"

namespace game
{
	constexpr uint32_t TAG_ELF = 0b1;
	constexpr uint32_t TAG_DRAGON = 0b10;
	constexpr uint32_t TAG_HUMAN = 0b100;
	constexpr uint32_t TAG_SLIME = 0b1000;
	constexpr uint32_t TAG_TOKEN = 0b10000;
	constexpr uint32_t TAG_GOBLIN = 0b100000;
	constexpr uint32_t TAG_ELEMENTAL = 0b1000000;

	struct MonsterCard final : Card
	{
		enum class Tag
		{
			elf = TAG_ELF,
			human = TAG_HUMAN,
			token = TAG_TOKEN,
			goblin = TAG_GOBLIN,
			elemental = TAG_ELEMENTAL
		};

		uint32_t attack = 1;
		uint32_t health = 2;
		uint32_t tags = 0;

		[[nodiscard]] Type GetType() override
		{
			return Type::monster;
		}
	};
}
