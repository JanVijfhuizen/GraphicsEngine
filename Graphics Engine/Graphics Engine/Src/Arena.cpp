#include "pch.h"
#include "Arena.h"
#include "JvMath.h"

namespace jv
{
	Arena Arena::Create(const ArenaCreateInfo& info)
	{
		Arena arena{};
		arena.info = info;
		arena.memory = info.memory ? info.memory : info.alloc(info.memorySize);
		return arena;
	}

	void Arena::Destroy(const Arena& arena)
	{
		if (arena.next)
			Destroy(*arena.next);
		if(!arena.info.memory)
			arena.info.free(arena.memory);
	}

	void* Arena::Alloc(const uint32_t size)
	{
		if (front + size + sizeof(ArenaAllocMetaData) > info.memorySize - sizeof(Arena))
		{
			if (!next)
			{
				const auto nextPtr = &static_cast<char*>(memory)[info.memorySize - sizeof(Arena)];
				next = reinterpret_cast<Arena*>(nextPtr);
				ArenaCreateInfo createInfo = info;
				createInfo.memory = nullptr;
				createInfo.memorySize = Max<uint32_t>(createInfo.memorySize, size + sizeof(ArenaAllocMetaData) + sizeof(Arena));
				*next = Create(createInfo);
			}
			return next->Alloc(size);
		}
		void* ptr = &static_cast<char*>(memory)[front];
		front += size + sizeof(ArenaAllocMetaData);
		const auto metaData = reinterpret_cast<ArenaAllocMetaData*>(&static_cast<char*>(memory)[front - sizeof(
			ArenaAllocMetaData)]);
		*metaData = ArenaAllocMetaData();
		metaData->size = size;

		return ptr;
	}

	void Arena::Free(const void* ptr)
	{
		const Arena* current = this;
		while(current)
		{
			if(current->front != 0)
			{
				const auto metaData = reinterpret_cast<ArenaAllocMetaData*>(&static_cast<char*>(memory)[front - sizeof(ArenaAllocMetaData)]);
				const auto frontPtr = &static_cast<char*>(memory)[front - sizeof(ArenaAllocMetaData) - metaData->size];
				if(frontPtr == ptr)
				{
					front -= metaData->size + sizeof(ArenaAllocMetaData);
					return;
				}
			}
			
			current = current->next;
		}
	}

	void Arena::Clear()
	{
		front = 0;
		if (next)
			next->Clear();
	}
}
