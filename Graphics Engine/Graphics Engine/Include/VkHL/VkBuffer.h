#pragma once
#include "Vk/VkMemory.h"

namespace jv::vk
{
	struct Buffer final
	{
		VkBuffer buffer;
		Memory memory;
		uint64_t memoryHandle;
	};
}
