#pragma once
#include "JLib/Array.h"
#include "JLib/LinkedList.h"

namespace jv::vk
{
	struct App;

	// Graphics memory.
	struct FreeMemory final
	{
		VkDeviceMemory memory;
		VkDeviceSize offset;
	};

	// Handles manual memory allocation for graphics memory.
	// Allocates one or more large chunks of memory which can then be (re)used for smaller linear allocations.
	struct FreeArena final
	{
		struct Page final
		{
			VkDeviceSize alignment;
			VkDeviceMemory memory;
			VkDeviceSize size;
			VkDeviceSize remaining;
		};

		struct Pool final
		{
			VkFlags memPropertyFlags;
			LinkedList<Page> pages{};
		};

		struct Memory final
		{
			struct Unpacked final
			{
				uint32_t size;
				uint16_t poolId;
				uint16_t pageNum;
			};

			union
			{
				uint64_t handle;
				Unpacked unpacked;
			};
		};
		
		VkDeviceSize pageSize;
		uint64_t scope;
		Array<Pool> pools;

		static FreeArena Create(Arena& arena, const App& app, VkDeviceSize pageSize = 4096);
		static void Destroy(Arena& arena, const App& app, const FreeArena& freeArena);
		
		[[nodiscard]] uint64_t Alloc(const App& app, Arena& arena, VkMemoryRequirements memRequirements, 
			VkMemoryPropertyFlags properties, uint32_t count, FreeMemory& outFreeMemory) const;
		void Free(uint64_t handle) const;
	};
}
