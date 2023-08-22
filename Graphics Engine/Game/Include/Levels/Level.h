#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/BossCard.h"
#include "Cards/EventCard.h"
#include "Cards/FlawCard.h"
#include "Cards/MagicCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/RoomCard.h"
#include "Engine/TaskSystem.h"
#include "JLib/Array.h"
#include "Tasks/DynamicRenderTask.h"
#include "Tasks/TextTask.h"
#include "Tasks/PixelPerfectRenderTask.h"

namespace jv::ge
{
	struct AtlasTexture;
}

namespace game
{
	struct InputState;
	struct BoardState;
	struct PlayerState;
	struct GameState;

	struct LevelInfo
	{
		jv::Arena& arena;
		jv::Arena& tempArena;
		jv::Arena& frameArena;
		const jv::ge::Resource scene;
		const jv::Array<jv::ge::AtlasTexture>& atlasTextures;

		GameState& gameState;
		PlayerState& playerState;

		const jv::Array<MonsterCard>& monsters;
		const jv::Array<ArtifactCard>& artifacts;
		const jv::Array<BossCard>& bosses;
		const jv::Array<RoomCard>& rooms;
		const jv::Array<MagicCard>& magics;
		const jv::Array<FlawCard>& flaws;
		const jv::Array<EventCard>& events;
	};

	struct LevelCreateInfo final : LevelInfo
	{

	};

	struct LevelUpdateInfo final : LevelInfo
	{
		glm::ivec2 resolution;
		const InputState& inputState;
		TaskSystem<RenderTask>& renderTasks;
		TaskSystem<DynamicRenderTask>& dynamicRenderTasks;
		TaskSystem<RenderTask>& priorityRenderTasks;
		TaskSystem<TextTask>& textTasks;
		TaskSystem<PixelPerfectRenderTask>& pixelPerfectRenderTasks;
		float deltaTime;
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
		};

		struct CardDrawInfo final
		{
			glm::ivec2 origin;
			Card* card;
			bool center = false;
			glm::vec4 borderColor{1};
			bool selectable = true;
			float lifeTime = 0;
		};

		struct CardSelectionDrawInfo final
		{
			Card** cards;
			Card*** stacks = nullptr;
			uint32_t* stackCounts = nullptr;
			uint32_t* outStackSelected = nullptr;
			const char** texts = nullptr;
			uint32_t length = 1;
			uint32_t height;
			uint32_t highlighted = -1;
			bool* selectedArr = nullptr;
			bool* greyedOutArr = nullptr;
			float lifeTime = -1;
			uint32_t rowCutoff = 8;
			int32_t offsetMod = 0;
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
		[[nodiscard]] bool DrawButton(const LevelUpdateInfo& info, const ButtonDrawInfo& drawInfo) const;
		uint32_t DrawCardSelection(const LevelUpdateInfo& info, const CardSelectionDrawInfo& drawInfo);
		bool DrawCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo);
		static bool CollidesCard(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo);
		void DrawFullCard(Card* card);
		void DrawTopCenterHeader(const LevelUpdateInfo& info, HeaderSpacing spacing, const char* text, uint32_t scale = 1, float overrideLifeTime = -1) const;
		void DrawPressEnterToContinue(const LevelUpdateInfo& info, HeaderSpacing spacing, float overrideLifeTime = -1) const;
		uint32_t DrawParty(const LevelUpdateInfo& info, uint32_t height, bool* selectedArr = nullptr);
		[[nodiscard]] static uint32_t GetSpacing(HeaderSpacing spacing);

		[[nodiscard]] float GetTime() const;
		[[nodiscard]] bool GetIsLoading() const;
		void Load(LevelIndex index);

	private:
		const float _LOAD_DURATION = 0;
		bool _loading;
		float _timeSinceLoading;
		LevelIndex _loadingLevelIndex;
		float _timeSinceOpened;
		Card* _fullCard;
	};
}
