#pragma once

[[nodiscard]] inline uint32_t GetNumberOfDigits(const uint32_t i)
{
	return i > 0 ? static_cast<int32_t>(log10(static_cast<double>(i))) + 1 : 1;
}

constexpr glm::ivec2 SIMULATED_RESOLUTION(320, 240);
constexpr float TEXT_DRAW_SPEED = 20;

constexpr uint32_t BOARD_CAPACITY_PER_SIDE = 5;
constexpr uint32_t BOARD_CAPACITY = BOARD_CAPACITY_PER_SIDE * 2;
constexpr uint32_t PARTY_CAPACITY = BOARD_CAPACITY_PER_SIDE;
constexpr uint32_t MONSTER_ARTIFACT_CAPACITY = 2;
constexpr uint32_t ROOM_COUNT_BEFORE_BOSS = 6;
constexpr uint32_t SPELL_DECK_SIZE = 12;
constexpr uint32_t DISCOVER_LENGTH = 3;
constexpr uint32_t SUB_BOSS_COUNT = 4;
constexpr uint32_t TOTAL_BOSS_COUNT = SUB_BOSS_COUNT + 1;
constexpr uint32_t SPELL_CARD_COPY_COUNT = 3;
constexpr uint32_t ROOMS_BEFORE_ROOM_EFFECTS = ROOM_COUNT_BEFORE_BOSS * 2;
constexpr uint32_t ROOMS_BEFORE_EVENT_EFFECTS = ROOM_COUNT_BEFORE_BOSS;

constexpr uint32_t HAND_INITIAL_SIZE = 3;
constexpr uint32_t HAND_MAX_SIZE = 4;
constexpr uint32_t MAX_MANA = 5;
constexpr uint32_t STACK_MAX_SIZE = 128;

constexpr float ACTION_STATE_DEFAULT_DURATION = .4f;
constexpr float START_OF_TURN_ACTION_STATE_DURATION = 2;
constexpr float CARD_ACTIVATION_DURATION = .25f;
constexpr float CARD_FADE_DURATION = ACTION_STATE_DEFAULT_DURATION;
constexpr float CARD_DRAW_DURATION = ACTION_STATE_DEFAULT_DURATION * .75f;
constexpr float STACK_OVERLOAD_DURATION = 3;
constexpr uint32_t STACK_OVERLOAD_THRESHOLD = 99;

constexpr uint32_t ALLY_HEIGHT = SIMULATED_RESOLUTION.y / 10 * 4.5;
constexpr uint32_t ENEMY_HEIGHT = ALLY_HEIGHT + 92;
constexpr uint32_t HAND_HEIGHT = 40;
constexpr uint32_t CENTER_HEIGHT = ALLY_HEIGHT + (ENEMY_HEIGHT - ALLY_HEIGHT) / 2;

constexpr uint32_t EVENT_CARD_MAX_COUNT = 1;

constexpr uint32_t META_DATA_ROOM_INDEX = 0;
constexpr uint32_t META_DATA_EVENT_INDEX = META_DATA_ROOM_INDEX + 1;
constexpr uint32_t META_DATA_HAND_INDEX = META_DATA_EVENT_INDEX + EVENT_CARD_MAX_COUNT;
constexpr uint32_t META_DATA_ALLY_INDEX = META_DATA_HAND_INDEX + HAND_MAX_SIZE;
constexpr uint32_t META_DATA_ENEMY_INDEX = META_DATA_ALLY_INDEX + BOARD_CAPACITY_PER_SIDE;

constexpr glm::ivec2 CARD_ART_SHAPE{ 32 };
constexpr glm::ivec2 LARGE_CARD_ART_SHAPE = CARD_ART_SHAPE * 2;
constexpr uint32_t CARD_ART_LENGTH = 5;
constexpr uint32_t LARGE_CARD_ART_LENGTH = 12;
constexpr float CARD_ANIM_SPEED = 8.f;

constexpr float CARD_HORIZONTAL_MOVE_SPEED = 1.f / (ACTION_STATE_DEFAULT_DURATION / 2);
constexpr float CARD_VERTICAL_MOVE_SPEED = ACTION_STATE_DEFAULT_DURATION / 2;

struct MONSTER_IDS
{
	enum
	{
		VULTURE,
		GOBLIN,
		DEMON,
		ELF,
		TREASURE,
		SLIME,
		DAISY,
		GOD,
		GREAT_TROLL,
		SLIME_QUEEN,
		LICH_KING,
		KNIFE_JUGGLER,
		ELVEN_SAGE,
		STORM_ELEMENTAL,
		MOSSY_ELEMENTAL,
		STONE_ELEMENTAL,
		GOBLIN_KING,
		GOBLIN_CHAMPION,
		UNSTABLE_GOLEM,
		MAIDEN_OF_THE_MOON,
		FLEETING_SOLDIER,
		MANA_CYCLONE,
		WOUNDED_TROLL,
		CHAOS_CLOWN,
		GOBLIN_SLINGER,
		GOBLIN_BOMB,
		GOBLIN_PARTY_STARTER,
		OBNOXIOUS_FAN,
		SLIME_SOLDIER,
		MAD_PYROMANCER,
		PHANTASM,
		GOBLIN_SCOUT,
		ACOLYTE_OF_PAIN,
		BEAST_MASTER,
		LIBRARIAN,
		MIRROR_KNIGHT,
		BOMBA,
		BOMB,
		THE_DREAD,
		ARCHMAGE,
		SHELLY,
		GOBLIN_QUEEN,
		MASTER_LICH,
		THE_COLLECTOR,
		SLIME_EMPEROR,
		LENGTH
	};
};

struct SPELL_IDS
{
	enum
	{
		ARCANE_INTELLECT,
		DREAD_SACRIFICE,
		TRANQUILIZE,
		DOUBLE_TROUBLE,
		GOBLIN_AMBUSH,
		WINDFALL,
		RALLY,
		HOLD_THE_LINE,
		RAMPANT_GROWTH,
		ENLIGHTENMENT,
		DRUIDIC_RITUAL,
		ASCENSION,
		STAMPEDE,
		ENRAGE,
		INFURIATE,
		GOBLIN_CHANT,
		TREASURE_HUNT,
		REWIND,
		MANA_SURGE,
		DARK_RITUAL,
		FLAME_BOLT,
		SHOCK,
		PYROBlAST,
		UNSTABLE_COPY,
		PERFECT_COPY,
		PAIN_FOR_GAIN,
		BETRAYAL,
		INCANTATION_OF_DOOM,
		PROTECT,
		SHIELD,
		GROUP_HUG,
		STALL,
		BALANCE,
		COUNTER_BALANCE,
		GRIT,
		PARIAH,
		FIRE_STORM,
		UNITY,
		TRIBALISM,
		LOYAL_WORKFORCE,
		PICK,
		CYCLE,
		LENGTH
	};
};

struct ROOM_IDS
{
	enum
	{
		FIELD_OF_VENGEANCE,
		FORSAKEN_BATTLEFIELD,
		BLESSED_HALLS,
		KHAALS_DOMAIN,
		ARENA_OF_THE_DAMNED,
		TRANQUIL_WATERS,
		PLAIN_MEADOWS,
		PRISON_OF_ETERNITY,
		LENGTH
	};
};

struct ARTIFACT_IDS
{
	enum
	{
		ARCANE_AMULET,
		THORNMAIL,
		REVERSE_CARD,
		FALSE_ARMOR,
		MAGE_ARMOR,
		MASK_OF_ETERNAL_YOUTH,
		CORRUPTING_KNIFE,
		SACRIFICIAL_ALTAR,
		BLOOD_AXE,
		RUSTY_CLAW,
		BLOOD_HAMMER,
		SPIKEY_COLLAR,
		BOOTS_OF_SWIFTNESS,
		BLESSED_RING,
		MANAMUNE,
		THORN_WHIP,
		RED_CLOTH,
		HELMET_OF_THE_HOST,
		RUSTY_SPEAR,
		RUSTY_COLLAR,
		SWORD_OF_SPELLCASTING,
		STAFF_OF_AEONS,
		STAFF_OF_SUMMONING,
		MANA_RING,
		LENGTH
	};
};

struct CURSE_IDS
{
	enum
	{
		FADING,
		WEAKNESS,
		COWARDICE,
		DUM_DUM,
		HATE,
		HAUNTING,
		TIME,
		VULNERABILITY,
		LENGTH
	};
};

struct EVENT_IDS
{
	enum
	{
		AETHER_SURGE,
		DOPPLEGANGSTERS,
		GATHERING_STORM,
		NO_YOU,
		GOBLIN_PLAGUE,
		WHIRLWIND,
		HEALING_WORD,
		BRIEF_RESPISE,
		CHASE_THE_DRAGON,
		LENGTH
	};
};

enum class LevelIndex
{
	mainMenu,
	newGame,
	main,
	gameOver,
	animOnly
};

enum class TextureId
{
	alphabet,
	largeAlphabet,
	numbers,
	symbols,
	mouse,
	card,
	stats,
	flowers,
	fallback,
	empty,
	manabar,
	manacrystal,
	length
};