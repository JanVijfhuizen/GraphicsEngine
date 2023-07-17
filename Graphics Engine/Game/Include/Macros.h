#pragma once

constexpr uint32_t PARTY_ACTIVE_CAPACITY = 4;
constexpr uint32_t PARTY_INACTIVE_CAPACITY = 2;
constexpr uint32_t PARTY_CAPACITY = PARTY_ACTIVE_CAPACITY + PARTY_INACTIVE_CAPACITY;
constexpr uint32_t BOARD_CAPACITY_PER_SIDE = 8;
constexpr uint32_t BOARD_CAPACITY = BOARD_CAPACITY_PER_SIDE * 2;
constexpr uint32_t MONSTER_ARTIFACT_CAPACITY = 4;
constexpr uint32_t MONSTER_FLAW_CAPACITY = 4;
constexpr uint32_t ROOM_COUNT_BEFORE_BOSS = 10;
constexpr uint32_t ROOM_COUNT_BEFORE_FLAW = ROOM_COUNT_BEFORE_BOSS / 2;
constexpr uint32_t MAGIC_CAPACITY = 24;
constexpr uint32_t DISCOVER_LENGTH = 3;
constexpr uint32_t SUB_BOSS_COUNT = 4;
constexpr uint32_t TOTAL_BOSS_COUNT = SUB_BOSS_COUNT + 1;

constexpr float CARD_SPACING = .1f;
constexpr float CARD_WIDTH = .15f;
constexpr float CARD_HEIGHT = .2f;
constexpr float CARD_PIC_FILL_HEIGHT = .6f;
constexpr float CARD_WIDTH_OFFSET = CARD_WIDTH * 2 + CARD_SPACING;
constexpr float CARD_HEIGHT_OFFSET = CARD_HEIGHT + CARD_SPACING;
constexpr float CARD_TEXT_SIZE = CARD_WIDTH * .1f;
constexpr float CARD_TITLE_SIZE = CARD_WIDTH * .12f;
constexpr float CARD_STAT_SIZE = CARD_TEXT_SIZE * 2;
constexpr float CARD_DARKENED_COLOR_MUL = .2f;
constexpr float CARD_BORDER_OFFSET = CARD_WIDTH / 8;
constexpr float CARD_HOVERED_SIZE_PCT_INCREASE = .1f;
constexpr float CARD_SELECTED_Y_POSITION_INCREASE = CARD_HEIGHT / 10;
constexpr float CARD_LARGE_SIZE_INCREASE_MUL = 2;

constexpr uint32_t CARD_SMALL_TEXT_CAPACITY = 8;

constexpr glm::vec2 TEXT_CENTER_TOP_POSITION = glm::vec2(0, -.8f);
constexpr glm::vec2 TEXT_CENTER_BOT_POSITION = TEXT_CENTER_TOP_POSITION * glm::vec2(1, -1);
constexpr float TEXT_BIG_SCALE = .06f;
constexpr float TEXT_MEDIUM_SCALE = .04f;
constexpr float TEXT_SMALL_SCALE = .02f;

constexpr float BUTTON_Y_SCALE = .12f;
constexpr float BUTTON_Y_OFFSET = .04f;
constexpr float BUTTON_X_DEFAULT_SCALE = .4f;

constexpr uint32_t MONSTER_STARTING_COMPANION_ID = 0;

enum class LevelIndex
{
	mainMenu,
	newGame,
	partySelect,
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