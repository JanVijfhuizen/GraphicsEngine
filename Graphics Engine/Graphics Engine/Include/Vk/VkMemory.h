#pragma once

namespace jv::vk
{
	// Graphics memory.
	struct Memory final
	{
		VkDeviceMemory memory;
		VkDeviceSize offset;
	};
}