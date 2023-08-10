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
			bool overflow = false;
			uint32_t scale = 2;
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
			Card* cards;
			uint32_t length;
			bool center = false;
		};

		virtual void Create(const LevelCreateInfo& info);
		virtual bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		virtual void PostUpdate(const LevelUpdateInfo& info);

		[[nodiscard]] void DrawHeader(const LevelUpdateInfo& info, const HeaderDrawInfo& drawInfo) const;
		[[nodiscard]] bool DrawButton(const LevelUpdateInfo& info, const ButtonDrawInfo& drawInfo) const;
		[[nodiscard]] uint32_t DrawCards(const LevelUpdateInfo& info, const CardDrawInfo& drawInfo) const;

		[[nodiscard]] float GetTime() const;
		[[nodiscard]] bool GetIsLoading() const;
		void Load(LevelIndex index);

	private:
		const float _LOAD_DURATION = 1;
		bool _loading;
		float _timeSinceLoading;
		LevelIndex _loadingLevelIndex;
		bool _lMousePressed = false;
		float _timeSinceOpened;
	};
}
