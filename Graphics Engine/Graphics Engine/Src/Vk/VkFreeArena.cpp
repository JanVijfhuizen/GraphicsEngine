#include "pch.h"
#include "Vk/VkFreeArena.h"
#include "JLib/ArrayUtils.h"
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

		return -1;
	}

	FreeArena FreeArena::Create(const FreeArenaInfo& info)
	{
		FreeArena freeArena{};
		freeArena.info = info;
		freeArena.scope = ArenaScope::Create(*info.arena);

		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(info.app->physicalDevice, &memProperties);
		freeArena.pools = CreateArray<Pool>(*info.arena, memProperties.memoryTypeCount);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
		{
			auto& pool = freeArena.pools[i];
			const auto& memType = memProperties.memoryTypes[i];
			pool.memPropertyFlags = memType.propertyFlags;
		}

		return freeArena;
	}

	void FreeArena::Destroy(const FreeArena& freeArena)
	{
		for (const auto& pool : freeArena.pools)
			for (const auto& page : pool.pages)
				vkFreeMemory(freeArena.info.app->device, page.memory, nullptr);
		ArenaScope::Destroy(freeArena.scope);
	}

	FreeMemory FreeArena::Alloc(const VkMemoryRequirements memRequirements, 
		const VkMemoryPropertyFlags properties,
		const uint32_t count) const
	{
		return {};
	}

	void FreeArena::Free(const FreeMemory& memory) const
	{
	}
}
