#pragma once

constexpr uint32_t PARTY_ACTIVE_CAPACITY = 4;
constexpr uint32_t PARTY_INACTIVE_CAPACITY = 2;
constexpr uint32_t PARTY_CAPACITY = PARTY_ACTIVE_CAPACITY + PARTY_INACTIVE_CAPACITY;
constexpr uint32_t BOARD_CAPACITY_PER_SIDE = 8;
constexpr uint32_t BOARD_CAPACITY = BOARD_CAPACITY_PER_SIDE * 2;
constexpr uint32_t MONSTER_ARTIFACT_CAPACITY = 4;
constexpr uint32_t MONSTER_FLAW_CAPACITY = 4;
constexpr uint32_t ROOM_COUNT_BEFORE_BOSS = 10;
constexpr uint32_t ROOM_COUNT_BEFORE_FLAW = 5;
constexpr uint32_t MAGIC_CAPACITY = 24;
constexpr uint32_t DISCOVER_LENGTH = 3;

constexpr float CARD_SPACING = .1f;
constexpr float CARD_WIDTH = .15f;
constexpr float CARD_HEIGHT = .2f;
constexpr float CARD_PIC_FILL_HEIGHT = .6f;
constexpr float CARD_WIDTH_OFFSET = CARD_WIDTH * 2 + CARD_SPACING;
constexpr float CARD_HEIGHT_OFFSET = CARD_HEIGHT + CARD_SPACING;
constexpr float CARD_TEXT_SIZE = CARD_WIDTH * .1f;
constexpr float CARD_DARKENED_COLOR_MUL = .2f;

enum class LevelIndex
{
	mainMenu,
	newGame,
	main
};

enum class TextureId
{
	alphabet,
	numbers,
	symbols,
	mouse,
	fallback,
	length
};