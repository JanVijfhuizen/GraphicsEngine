#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/QuestCard.h"
#include "JLib/Array.h"

namespace game
{
	struct Library final
	{
		jv::Array<MonsterCard> monsters;
		jv::Array<ArtifactCard> artifacts;
		jv::Array<QuestCard> quests;
	};
}