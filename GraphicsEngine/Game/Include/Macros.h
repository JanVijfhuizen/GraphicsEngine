#pragma once

[[nodiscard]] inline uint32_t GetNumberOfDigits(const uint32_t i)
{
	return i > 0 ? static_cast<int32_t>(log10(static_cast<double>(i))) + 1 : 1;
}

constexpr glm::ivec2 SIMULATED_RESOLUTION(320, 240);
constexpr float TEXT_DRAW_SPEED = 20;

constexpr uint32_t PARTY_ACTIVE_CAPACITY = 4;
constexpr uint32_t PARTY_INACTIVE_CAPACITY = 2;
constexpr uint32_t PARTY_CAPACITY = PARTY_ACTIVE_CAPACITY + PARTY_INACTIVE_CAPACITY;
constexpr uint32_t BOARD_CAPACITY_PER_SIDE = 6;
constexpr uint32_t BOARD_CAPACITY = BOARD_CAPACITY_PER_SIDE * 2;
constexpr uint32_t MONSTER_ARTIFACT_CAPACITY = 4;
constexpr uint32_t MONSTER_FLAW_CAPACITY = 4;
constexpr uint32_t ROOM_COUNT_BEFORE_BOSS = 10;
constexpr uint32_t ROOM_COUNT_BEFORE_FLAW = ROOM_COUNT_BEFORE_BOSS / 2;
constexpr uint32_t MAGIC_DECK_SIZE = 24;
constexpr uint32_t DISCOVER_LENGTH = 3;
constexpr uint32_t SUB_BOSS_COUNT = 4;
constexpr uint32_t TOTAL_BOSS_COUNT = SUB_BOSS_COUNT + 1;

constexpr uint32_t HAND_INITIAL_SIZE = 3;
constexpr uint32_t HAND_MAX_SIZE = 5;
constexpr uint32_t MAX_MANA = 7;
constexpr uint32_t STACK_MAX_SIZE = 128;

constexpr uint32_t MONSTER_STARTING_COMPANION_ID = 0;
constexpr float ACTION_STATE_DEFAULT_DURATION = .4f;
constexpr float START_OF_TURN_ACTION_STATE_DURATION = 2;
constexpr float CARD_ACTIVATION_DURATION = .25f;
constexpr float CARD_FADE_DURATION = ACTION_STATE_DEFAULT_DURATION;
constexpr float CARD_DRAW_DURATION = ACTION_STATE_DEFAULT_DURATION * .75f;

constexpr uint32_t ALLY_HEIGHT = SIMULATED_RESOLUTION.y / 10 * 4;
constexpr uint32_t ENEMY_HEIGHT = ALLY_HEIGHT + 92;
constexpr uint32_t HAND_HEIGHT = 32;
constexpr uint32_t CENTER_HEIGHT = ALLY_HEIGHT + (ENEMY_HEIGHT - ALLY_HEIGHT) / 2;

constexpr uint32_t FINAL_BOSS_ID = 16;

constexpr uint32_t META_DATA_ROOM_INDEX = 0;
constexpr uint32_t META_DATA_EVENT_INDEX = META_DATA_ROOM_INDEX + 1;
constexpr uint32_t META_DATA_HAND_INDEX = META_DATA_EVENT_INDEX + 1;
constexpr uint32_t META_DATA_ALLY_INDEX = META_DATA_HAND_INDEX + HAND_MAX_SIZE;
constexpr uint32_t META_DATA_ENEMY_INDEX = META_DATA_ALLY_INDEX + BOARD_CAPACITY_PER_SIDE;

constexpr glm::ivec2 CARD_ART_SHAPE{ 16 };
constexpr uint32_t CARD_ART_LENGTH = 2;
constexpr float CARD_ANIM_SPEED = .5f;

constexpr float CARD_HORIZONTAL_MOVE_SPEED = 1.f / (ACTION_STATE_DEFAULT_DURATION / 2);
constexpr float CARD_VERTICAL_MOVE_SPEED = ACTION_STATE_DEFAULT_DURATION / 2;

enum class LevelIndex
{
	mainMenu,
	newGame,
	partySelect,
	main,
	gameOver,
	animOnly
};

enum class TextureId
{
	alphabet,
	numbers,
	symbols,
	mouse,
	card,
	cardLarge,
	button,
	stats,
	fallback,
	empty,
	buttonSmall,
	length
};