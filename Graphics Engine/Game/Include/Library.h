#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/QuestCard.h"
#include "JLib/Array.h"

namespace game
{
	struct Library final
	{
		uint64_t scope;
		jv::Array<MonsterCard> monsters;
		jv::Array<ArtifactCard> artifacts;
		jv::Array<QuestCard> quests;

		static Library Create(jv::Arena& arena);
		static void Destroy(jv::Arena& arena, const Library& library);
	};
}