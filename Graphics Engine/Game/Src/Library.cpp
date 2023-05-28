#include "pch_game.h"
#include "Library.h"

#include "JLib/ArrayUtils.h"

namespace game
{
	Library Library::Create(jv::Arena& arena)
	{
		Library library{};
		library.scope = arena.CreateScope();

		library.monsters = jv::CreateArray<MonsterCard>(arena, 30);

		// temp.
		for (auto& monster : library.monsters)
		{
			monster.tier = 1 + rand() % 7;
			monster.unique = false;
		}

		// temp.
		for (int i = 0; i < NEW_GAME_MONSTER_SELECTION_COUNT; ++i)
		{
			library.monsters[i].tier = 1;
		}

		library.artifacts = jv::CreateArray<ArtifactCard>(arena, 30);
		for (auto& artifact : library.artifacts)
		{
			artifact.tier = 1 + rand() % 7;
			artifact.unique = false;
		}

		// temp.
		for (int i = 0; i < NEW_GAME_ARTIFACT_SELECTION_COUNT; ++i)
		{
			library.artifacts[i].tier = 1;
		}

		library.quests = jv::CreateArray<QuestCard>(arena, 30);
		for (auto& quest : library.quests)
		{
			quest.tier = rand() % 8;
		}

		// temp.
		for (int i = 0; i < NEW_RUN_QUEST_SELECTION_COUNT; ++i)
		{
			library.quests[i].tier = 1;
		}

		return library;
	}

	void Library::Destroy(jv::Arena& arena, const Library& library)
	{
		arena.DestroyScope(library.scope);
	}
}
