#pragma once

[[nodiscard]] inline uint32_t GetNumberOfDigits(const uint32_t i)
{
	return i > 0 ? static_cast<int32_t>(log10(static_cast<double>(i))) + 1 : 1;
}

constexpr uint32_t DAISY_MOD_STATS = 3;

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
constexpr uint32_t HAND_MAX_SIZE = 5;
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
constexpr uint32_t CARD_ART_MAX_LENGTH = 12;
constexpr uint32_t LARGE_CARD_ART_MAX_LENGTH = 12;
constexpr float CARD_ANIM_SPEED = 8.f;

constexpr float CARD_HORIZONTAL_MOVE_SPEED = 1.f / (ACTION_STATE_DEFAULT_DURATION / 2);
constexpr float CARD_VERTICAL_MOVE_SPEED = ACTION_STATE_DEFAULT_DURATION / 2;

const constexpr char* SOUND_BACKGROUND_MUSIC = "Audio/battle_music.wav";
constexpr float AUDIO_BACKGROUND_VOLUME = .3f;

struct MONSTER_IDS
{
	enum
	{
		VULTURE,
		GOBLIN,
		DEMON,
		ELF,
		TREASURE_GOBLIN,
		SLIME,
		DAISY,
		DARK_CRESCENT,
		LORD_OF_FLAME,
		SLIME_FATHER,
		GHOSTFLAME_PONTIFF,
		GOBLIN_QUEEN,
		KNIFE_JUGGLER,
		ELVEN_SAGE,
		STORM_ELEMENTAL,
		MOSSY_ELEMENTAL,
		FOREST_SPIRIT,
		GOBLIN_KING,
		GOBLIN_CHAMPION,
		UNSTABLE_CREATION,
		MOON_ACOLYTE,
		BERSERKER,
		MANA_DEVOURER,
		WOUNDED_PANDANA,
		CHAOS_CLOWN,
		DUNG_LOBBER,
		GOBLIN_BOMBER,
		GOBLIN_PRINCESS,
		PESKY_PARASITE,
		SLIME_SOLDIER,
		MAD_PYROMANCER,
		PHANTASM,
		GNOME_SCOUT,
		SLIME_HEAD,
		BEAST_SPIRIT,
		LIBRARIAN,
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
		PYROBLAST,
		UNSTABLE_COPY,
		PERFECT_COPY,
		PAIN_FOR_GAIN,
		BETRAYAL,
		INCANTATION_OF_DOOM,
		PROTECT,
		SHIELD,
		ENCITEMENT,
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
		DOMAIN_OF_PAIN,
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
		MASK_OF_THE_EMPEROR,
		MAGE_HAT,
		MASK_OF_YOUTH,
		CORRUPTING_KNIFE,
		CUP_OF_BLOOD,
		BLOOD_AXE,
		QUICK_BOW,
		MOON_SCYTE,
		JESTER_HAT,
		BOOTS_OF_SWIFTNESS,
		BLESSED_RING,
		FORBIDDEN_TOME,
		THORN_WHIP,
		MASK_OF_CHAOS,
		HELMET_OF_HATE,
		MOANING_ORB,
		THE_BRAND,
		SWORD_OF_SPELLCASTING,
		CRYSTAL_FLOWER,
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
		RETRIBUTION,
		GOBLIN_PLAGUE,
		WHIRLWIND,
		HEALING_WORD,
		BRIEF_RESPISE,
		CHASE_THE_DRAGON,
		LENGTH
	};
};

constexpr uint32_t SPELL_ID_START = MONSTER_IDS::LENGTH;
constexpr uint32_t ARTIFACT_ID_START = SPELL_ID_START + SPELL_IDS::LENGTH;
constexpr uint32_t ROOM_ID_START = ARTIFACT_ID_START + ARTIFACT_IDS::LENGTH;
constexpr uint32_t CURSE_ID_START = ROOM_ID_START + ROOM_IDS::LENGTH;
constexpr uint32_t EVENT_ID_START = CURSE_ID_START + CURSE_IDS::LENGTH;
constexpr uint32_t ALL_ID_COUNT = EVENT_ID_START + EVENT_IDS::LENGTH;

enum class LevelIndex
{
	mainMenu,
	newGame,
	main,
	gameOver,
	animOnly,
	animOnlyNoTimeReset
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
	manabarHollow,
	manacrystal,
	moon,
	cardSmall,
	textBubble,
	textBubbleTail,
	largeNumbers,
	title,
	pauseMenu,
	gameOver,
	length
};