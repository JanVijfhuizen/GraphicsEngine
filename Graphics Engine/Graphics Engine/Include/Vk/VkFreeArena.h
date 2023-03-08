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

	struct FreeMemory final
	{
		VkDeviceMemory memory;
		VkDeviceSize size;
		VkDeviceSize offset;
		uint32_t poolId;
	};

	struct FreeArena final
	{
		struct Page final
		{
			uint32_t size;
			uint32_t alignment;
			VkDeviceMemory memory = VK_NULL_HANDLE;
		};

		struct Pool final
		{
			VkFlags memPropertyFlags;
			LinkedList<Page> pages{};
		};

		FreeArenaInfo info;
		ArenaScope scope;
		Array<Pool> pools;

		static FreeArena Create(const FreeArenaInfo& info);
		static void Destroy(const FreeArena& freeArena);
		
		[[nodiscard]] FreeMemory Alloc(VkMemoryRequirements memRequirements, VkMemoryPropertyFlags properties, uint32_t count = 1) const;
		void Free(const FreeMemory& memory) const;
	};
}
