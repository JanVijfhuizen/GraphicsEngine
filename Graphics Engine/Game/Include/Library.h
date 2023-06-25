#pragma once
#include "Cards/ArtifactCard.h"
#include "Cards/MonsterCard.h"
#include "Cards/QuestCard.h"
#include "Cards/RoomCard.h"
#include "JLib/Array.h"
#include "JLib/Array.h"

namespace game
{
	struct Library final
	{
		jv::Array<MonsterCard> monsters;
		jv::Array<ArtifactCard> artifacts;
		jv::Array<QuestCard> quests;
		jv::Array<RoomCard> rooms;
	};
}