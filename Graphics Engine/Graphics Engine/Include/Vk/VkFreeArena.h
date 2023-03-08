#pragma once
#include "JLib/Array.h"
#include "JLib/LinkedList.h"

namespace jv::vk
{
	struct App;

	struct FreeArenaInfo final
	{
		Arena* arena;
		App* app;
		uint32_t pageSize = 4096;
	};

	struct FreeArena final
	{
		struct Pool final
		{
			VkFlags memPropertyFlags;
		};

		struct Page final
		{
			uint32_t size;
		};

		FreeArenaInfo info;
		ArenaScope scope;
		Array<Pool> pools;
		LinkedList<Page> pages{};

		static FreeArena Create(const FreeArenaInfo& info);
		static void Destroy(const FreeArena& freeArena);
	};
}
