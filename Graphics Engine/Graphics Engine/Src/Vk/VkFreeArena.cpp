#include "pch.h"
#include "Vk/VkFreeArena.h"
#include "JLib/ArrayUtils.h"
#include "Vk/VkApp.h"

namespace jv::vk
{
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
		ArenaScope::Destroy(freeArena.scope);
	}
}
