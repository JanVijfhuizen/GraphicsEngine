#pragma once
#include "Card.h"

namespace game
{
	constexpr uint32_t TAG_ELF = 0x1;
	constexpr uint32_t TAG_DRAGON = 0x1;
	constexpr uint32_t TAG_HUMAN = 0x10;
	constexpr uint32_t TAG_SLIME = 0x100;
	constexpr uint32_t TAG_TOKEN = 0x1000;

	struct MonsterCard final : Card
	{
		enum class Tag
		{
			elf = TAG_ELF,
			dragon = TAG_DRAGON,
			human = TAG_HUMAN,
			slime = TAG_SLIME,
			token = TAG_TOKEN
		};

		uint32_t attack = 1;
		uint32_t armorClass = 2;
		uint32_t health = 2;
		uint32_t tags = 0;
	};
}
