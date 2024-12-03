#include "pch.h"
#include "Vk/VkFreeArena.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/Math.h"
#include "Vk/VkApp.h"
#include "Vk/VkMemory.h"
#include <iostream>
#include <bitset>

namespace jv::vk
{
	uint32_t GetPoolId(const FreeArena& arena, const uint32_t typeFilter, const VkMemoryPropertyFlags properties)
	{
		uint32_t id = 0;
		for (const auto& pool : arena.pools)
		{
			bool typeValid = typeFilter & 1 << id, flagsValid = (pool.memPropertyFlags & properties) == properties;
			if (typeValid && flagsValid)
				return id;
			++id;
		}

		std::bitset<sizeof(properties)> propbits(properties);
		std::bitset < sizeof (typeFilter) > typebits(typeFilter);
		std::cout << "GetPoolId fail: ";
		std::cout << propbits << " " << typebits << "\n";
		return UINT32_MAX;
	}

	VkDeviceSize CalculateBufferSize(const VkDeviceSize size, const VkDeviceSize alignment)
	{
		return (size / alignment + (size % alignment > 0)) * alignment;
	}

	FreeArena FreeArena::Create(Arena& arena, const App& app, const VkDeviceSize pageSize)
	{
		std::cout << "Creating Arena\n";
		FreeArena freeArena{};
		freeArena.pageSize = pageSize;
		freeArena.scope = arena.CreateScope();

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(app.physicalDevice, &memProperties);
		freeArena.pools = CreateArray<Pool>(arena, memProperties.memoryTypeCount);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			auto& pool = freeArena.pools[i] = {};
			const auto& memType = memProperties.memoryTypes[i];
			pool.memPropertyFlags = memType.propertyFlags;
			std::bitset<sizeof(pool.memPropertyFlags) * 8> bits(pool.memPropertyFlags);
			std::cout << "Created pool with flags " << bits << "\n";
		}

		
		return freeArena;
	}

	void FreeArena::Destroy(Arena& arena, const App& app, const FreeArena& freeArena)
	{
		for (const auto& pool : freeArena.pools)
			for (const auto& page : pool.pages)
				vkFreeMemory(app.device, page.memory, nullptr);
		arena.DestroyScope(freeArena.scope);
	}

	uint64_t FreeArena::Alloc(Arena& arena, const App& app, const VkMemoryRequirements memRequirements,
	    const VkMemoryPropertyFlags properties,
		const uint32_t count, Memory& outMemory) const
	{
		const uint32_t poolId = GetPoolId(*this, memRequirements.memoryTypeBits, properties);
		assert(poolId != UINT32_MAX);

		const VkDeviceSize size = CalculateBufferSize(memRequirements.size * count, memRequirements.alignment);
		auto& pool = pools[poolId];

		Page* dstPage = nullptr;
		uint32_t pageNum = 0;
		for (auto& page : pool.pages)
		{
			if (page.remaining == page.size && page.alignment == memRequirements.alignment)
			{
				dstPage = &page;
				break;
			}

			++pageNum;
		}

		if(!dstPage)
		{
			dstPage = &Insert(arena, pool.pages, 0);
			dstPage->alignment = memRequirements.alignment;
			dstPage->remaining = Max<VkDeviceSize>(size, pageSize);
			dstPage->size = dstPage->remaining;

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = dstPage->size;
			allocInfo.memoryTypeIndex = static_cast<uint32_t>(poolId);

			const auto result = vkAllocateMemory(app.device, &allocInfo, nullptr, &dstPage->memory);
			assert(!result);
		}

		outMemory.memory = dstPage->memory;
		outMemory.offset = dstPage->size - dstPage->remaining;
		outMemory.size = size;

		dstPage->remaining -= size;

		FreeMemory handle{};
		handle.unpacked.size = static_cast<uint32_t>(size);
		handle.unpacked.pageNum = static_cast<uint16_t>(pool.pages.GetCount() - pageNum - 1);
		handle.unpacked.poolId = static_cast<uint16_t>(poolId);

		return handle.handle;
	}

	void FreeArena::Free(const uint64_t handle) const
	{
		FreeMemory memory{};
		memory.handle = handle;
		const auto& pool = pools[memory.unpacked.poolId];
		auto& page = pool.pages[memory.unpacked.pageNum];
		page.remaining += memory.unpacked.size;
	}
}
