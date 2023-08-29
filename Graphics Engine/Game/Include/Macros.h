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
constexpr uint32_t BOARD_CAPACITY_PER_SIDE = 5;
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

constexpr uint32_t CARD_MONSTER_ANIM_LENGTH = 4;
constexpr glm::ivec2 CARD_MONSTER_FRAME_SIZE(32);

constexpr uint32_t MONSTER_STARTING_COMPANION_ID = 0;
constexpr float ACTION_STATE_DEFAULT_DURATION = .4f;
constexpr float START_OF_TURN_ACTION_STATE_DURATION = .4f;
constexpr float ATTACK_ACTION_STATE_DURATION = .4f;

enum class LevelIndex
{
	mainMenu,
	newGame,
	partySelect,
	main,
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

enum class DynTextureId
{
	monster,
	mage,
	length
};