#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/EventCard.h"
#include "Cards/CurseCard.h"
#include "Cards/SpellCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/RoomCard.h"
#include "Engine/TaskSystem.h"
#include "Engine/TextureStreamer.h"
#include "JLib/Array.h"
#include "States/BoardState.h"
#include "Tasks/LightTask.h"
#include "Tasks/TextTask.h"
#include "Tasks/PixelPerfectRenderTask.h"

struct ma_engine;

namespace jv::ge
{
	struct AtlasTexture;
}

namespace game
{
	struct InputState;
	struct BoardState;
	struct GameState;

	struct LevelInfo
	{
		jv::Arena& arena;
		jv::Arena& tempArena;
		jv::Arena& frameArena;
		const jv::ge::Resource scene;
		const jv::Array<jv::ge::AtlasTexture>& atlasTextures;

		GameState& gameState;

		const jv::Array<MonsterCard>& monsters;
		const jv::Array<ArtifactCard>& artifacts;
		const jv::Array<uint32_t>& bosses;
		const jv::Array<RoomCard>& rooms;
		const jv::Array<SpellCard>& spells;
		const jv::Array<CurseCard>& curses;
		const jv::Array<EventCard>& events;
	};

	struct LevelCreateInfo final : LevelInfo
	{

	};

	struct LevelUpdateInfo final : LevelInfo
	{
		struct ScreenShakeInfo final
		{
			float fallOfThreshold = 1;
			float remaining = -1e5;
			float timeOut = ACTION_STATE_DEFAULT_DURATION;
			uint32_t intensity = 4;

			[[nodiscard]] bool IsInTimeOut() const;
		};

		glm::ivec2 resolution;
		const InputState& inputState;
		TaskSystem<TextTask>& textTasks;
		TaskSystem<PixelPerfectRenderTask>& renderTasks;
		TaskSystem<LightTask>& lightTasks;
		float deltaTime;
		TextureStreamer& textureStreamer;
		TextureStreamer& largeTextureStreamer;
		ScreenShakeInfo& screenShakeInfo;
		float& pixelation;
		bool& activePlayer;
		bool& inCombat;
		bool& isFullScreen;
		ma_engine& audioEngine;
	};

	struct Level
	{
		struct HeaderDrawInfo final
		{
			glm::ivec2 origin;
			const char* text;
			bool center = false;
			uint32_t lineLength = 32;
			uint32_t scale = 2;
			float overrideLifeTime = -1;
		};

		struct ButtonDrawInfo final
		{
			glm::ivec2 origin;
			const char* text;
			bool center = false;
			bool centerText = false;
			uint32_t width = 64;
			bool largeFont = false;
			bool drawLineByDefault = true;
		};

		struct CardDrawMetaData final
		{
			float hoverDuration = 0;
			float timeSinceStatsChanged = -1;
			glm::ivec2 mouseOffset{};
			bool dragging = false;
		};

		struct CardDrawInfo final
		{
			glm::ivec2 origin;
			Card* card;
			bool center = true;
			glm::vec4 bgColor{0, 0, 0, 1};
			glm::vec4 fgColor{ 1 };
			bool selectable = true;
			float lifeTime = -1;
			CombatStats* combatStats = nullptr;
			uint32_t cost = -1;
			bool ignoreAnim = false;
			float activationLerp = -1;
			CardDrawMetaData* metaData = nullptr;
			bool priority = false;
			uint32_t scale = 1;
			uint32_t target = -1;
			bool large = false;
		};

		struct CardSelectionDrawInfo final
		{
			Card** cards;
			Card*** stacks = nullptr;
			uint32_t* stackCounts = nullptr;
			uint32_t* outStackSelected = nullptr;
			const char** texts = nullptr;
			const uint32_t* targets = nullptr;
			uint32_t length = 1;
			uint32_t height;
			uint32_t highlighted = -1;
			bool* selectedArr = nullptr;
			bool* greyedOutArr = nullptr;
			float lifeTime = -1;
			uint32_t rowCutoff = 8;
			int32_t offsetMod = 8;
			CombatStats* combatStats = nullptr;
			uint32_t* costs = nullptr;
			glm::ivec2 overridePos;
			uint32_t overridePosIndex = -1;
			uint32_t centerOffset = 0;
			uint32_t damagedIndex = -1;
			CardDrawMetaData* metaDatas = nullptr;
			bool spawning = false;
			float spawnLerp = 0;
			bool spawnRight = true;
			uint32_t fadeIndex = -1;
			float fadeLerp;
			uint32_t activationIndex = -1;
			float activationLerp;
			bool mirrorHorizontal = false;
			bool draggable = false;
			bool selectable = true;
			bool containsBoss = false;
		};

		struct PartyDrawInfo final
		{
			uint32_t height = 0;
			bool* selectedArr = nullptr;
			bool* greyedOutArr = nullptr;
		};

		enum class HeaderSpacing
		{
			normal,
			close,
			far
		};

		virtual void Create(const LevelCreateInfo& info);
		virtual bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		virtual void PostUpdate(const LevelUpdateInfo& info);

		void DrawHeader(const LevelUpdateInfo& info, const HeaderDrawInfo& drawInfo) const;
		[[nodiscard]] bool DrawButton(const LevelUpdateInfo& info, const ButtonDrawInfo& drawInfo, float overrideLifetime = -1);
		[[nodiscard]] static glm::ivec2 GetCardShape(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo);
		[[nodiscard]] static glm::ivec2 GetCardPosition(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo, uint32_t i);
		uint32_t DrawCardSelection(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo);
		bool DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo);
		static bool CollidesCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo);
		void DrawFullCard(Card* card);
		void DrawSelectedCard(Card* card);
		void DrawTopCenterHeader(const LevelUpdateInfo& info, HeaderSpacing spacing, const char* text, uint32_t scale = 1, float overrideLifeTime = -1) const;
		void DrawPressEnterToContinue(const LevelUpdateInfo& info, HeaderSpacing spacing, float overrideLifeTime = -1) const;
		[[nodiscard]] static uint32_t GetSpacing(HeaderSpacing spacing);
		[[nodiscard]] static CombatStats GetCombatStat(const MonsterCard& card);

		[[nodiscard]] float GetTime() const;
		[[nodiscard]] bool GetIsLoading() const;
		void Load(LevelIndex index, bool animate);

	private:
		const float _LOAD_DURATION = .2f;
		bool _loading;
		float _timeSinceLoading;
		LevelIndex _loadingLevelIndex;
		bool _animateLoading;
		float _timeSinceOpened;

		Card* _fullCard;
		float _fullCardLifeTime;

		Card* _selectedCard;
		float _selectedCardLifeTime;
		float _buttonLifetime;
		bool _buttonHovered;
		bool _buttonHoveredLastFrame;

		CardDrawMetaData _cardDrawMetaDatas[PARTY_CAPACITY];
	};
}
