#include "pch.h"
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

	uint64_t FreeArena::Alloc(Arena& arena, const VkMemoryRequirements memRequirements,
		const VkMemoryPropertyFlags properties,
		const uint32_t count, FreeMemory& outFreeMemory) const
	{
		const uint32_t poolId = GetPoolId(*this, memRequirements.memoryTypeBits, properties);
		assert(poolId != UINT32_MAX);

		const VkDeviceSize size = CalculateBufferSize(memRequirements.size * count, memRequirements.alignment);
		auto& pool = pools[poolId];

		Page* dstPage = pool.pages.GetCount() > pool.depth ? &pool.pages[pool.depth] : nullptr;
		if (dstPage && dstPage->remaining < size)
			dstPage = nullptr;

		if(!dstPage)
		{
			dstPage = &Add(arena, pool.pages);
			dstPage->remaining = Max<VkDeviceSize>(size, pageSize);
		}

		outFreeMemory.memory = dstPage->memory;
		outFreeMemory.offset = dstPage->size - dstPage->remaining;

		dstPage->remaining -= size;

		// 32: Pool Id.
		// 32: Size.
		uint64_t handle = poolId;
		handle |= static_cast<uint64_t>(size) << 32;

		return handle;
	}

	void FreeArena::Free(const uint64_t handle) const
	{
	}
}
