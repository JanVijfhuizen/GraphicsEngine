﻿#include "pch.h"
#include "Vk/VkFreeArena.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/Math.h"
#include "Vk/VkApp.h"

namespace jv::vk
{
	uint32_t GetPoolId(const FreeArena& arena, const uint32_t typeFilter, const VkMemoryPropertyFlags properties)
	{
		uint32_t id = 0;
		for (const auto& pool : arena.pools)
		{
			if (typeFilter & 1 << id)
				if ((pool.memPropertyFlags & properties) == properties)
					return id;
			++id;
		}

		return UINT32_MAX;
	}

	VkDeviceSize CalculateBufferSize(const VkDeviceSize size, const VkDeviceSize alignment)
	{
		return (size / alignment + (size % alignment > 0)) * alignment;
	}

	FreeArena FreeArena::Create(Arena& arena, const App& app, const VkDeviceSize pageSize)
	{
		FreeArena freeArena{};
		freeArena.pageSize = pageSize;
		freeArena.scope = ArenaScope::Create(arena);

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(app.physicalDevice, &memProperties);
		freeArena.pools = CreateArray<Pool>(arena, memProperties.memoryTypeCount);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			auto& pool = freeArena.pools[i];
			const auto& memType = memProperties.memoryTypes[i];
			pool.memPropertyFlags = memType.propertyFlags;
		}

		return freeArena;
	}

	void FreeArena::Destroy(const App& app, const FreeArena& freeArena)
	{
		for (const auto& pool : freeArena.pools)
			for (const auto& page : pool.pages)
				vkFreeMemory(app.device, page.memory, nullptr);
		ArenaScope::Destroy(freeArena.scope);
	}

	uint64_t FreeArena::Alloc(const App& app, Arena& arena, const VkMemoryRequirements memRequirements,
	    const VkMemoryPropertyFlags properties,
		const uint32_t count, FreeMemory& outFreeMemory) const
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
			dstPage->remaining = Max<VkDeviceSize>(size, pageSize);
			dstPage->size = dstPage->remaining;

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = dstPage->size;
			allocInfo.memoryTypeIndex = static_cast<uint32_t>(poolId);

			const auto result = vkAllocateMemory(app.device, &allocInfo, nullptr, &dstPage->memory);
			assert(!result);
		}

		outFreeMemory.memory = dstPage->memory;
		outFreeMemory.offset = dstPage->size - dstPage->remaining;

		dstPage->remaining -= size;

		Handle handle{};
		handle.unpacked.size = static_cast<uint32_t>(size);
		handle.unpacked.pageNum = static_cast<uint16_t>(pool.pages.GetCount() - pageNum);
		handle.unpacked.poolId = static_cast<uint16_t>(poolId);

		return handle.handle;
	}

	void FreeArena::Free(const uint64_t handle) const
	{
		Handle _handle{};
		_handle.handle = handle;
		auto& page = pools[_handle.unpacked.poolId].pages[_handle.unpacked.pageNum];
		page.remaining += _handle.unpacked.size;
	}
}
