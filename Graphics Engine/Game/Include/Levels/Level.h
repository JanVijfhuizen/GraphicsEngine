#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/BossCard.h"
#include "Cards/FlawCard.h"
#include "Cards/MagicCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/RoomCard.h"
#include "Engine/TaskSystem.h"
#include "JLib/Array.h"
#include "JLib/Vector.h"
#include "Tasks/DynamicRenderTask.h"
#include "Tasks/TextTask.h"

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

		GameState& gameState;
		PlayerState& playerState;
		BoardState& boardState;

		const jv::Array<MonsterCard>& monsters;
		const jv::Array<ArtifactCard>& artifacts;
		const jv::Array<BossCard>& bosses;
		const jv::Array<RoomCard>& rooms;
		const jv::Array<MagicCard>& magics;
		const jv::Array<FlawCard>& flaws;

		jv::Vector<uint32_t>& monsterDeck;
		jv::Vector<uint32_t>& artifactDeck;
	};

	struct LevelCreateInfo final : LevelInfo
	{

	};

	struct LevelUpdateInfo final : LevelInfo
	{
		const InputState& inputState;
		TaskSystem<RenderTask>& renderTasks;
		TaskSystem<DynamicRenderTask>& dynamicRenderTasks;
		TaskSystem<TextTask>& textTasks;
		const jv::Array<jv::ge::SubTexture>& subTextures;
	};

	struct Level
	{
		virtual void Create(const LevelCreateInfo& info) = 0;
		virtual bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex) = 0;
	};
}
