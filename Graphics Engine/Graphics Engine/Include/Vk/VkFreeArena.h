#pragma once
#include "JLib/Array.h"
#include "JLib/LinkedList.h"

namespace jv::vk
{
	struct App;

	struct FreeMemory final
	{
		VkDeviceMemory memory;
		VkDeviceSize offset;
	};

	struct FreeArena final
	{
		struct Division final
		{
			VkDeviceSize size;
		};

		struct Page final
		{
			VkDeviceSize alignment;
			VkDeviceMemory memory = VK_NULL_HANDLE;
			VkDeviceSize size;
			VkDeviceSize remaining;
		};

		struct Pool final
		{
			VkFlags memPropertyFlags;
			LinkedList<Page> pages{};
			uint32_t depth = 0;
		};
		
		VkDeviceSize pageSize;
		ArenaScope scope;
		Array<Pool> pools;

		static FreeArena Create(Arena& arena, const App& app, VkDeviceSize pageSize = 4096);
		static void Destroy(const App& app, const FreeArena& freeArena);
		
		[[nodiscard]] uint64_t Alloc(Arena& arena, VkMemoryRequirements memRequirements, 
			VkMemoryPropertyFlags properties, uint32_t count, FreeMemory& outFreeMemory) const;
		void Free(uint64_t handle) const;
	};
}
