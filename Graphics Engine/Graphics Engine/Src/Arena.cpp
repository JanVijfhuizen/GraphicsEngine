#include "pch.h"
#include "Arena.h"
#include "JvMath.h"

namespace jv
{
	Arena Arena::Create(const ArenaCreateInfo& info)
	{
		Arena arena{};
		arena.info = info;
		arena.memory = info.alloc(info.chunkSize);
		return arena;
	}

	void Arena::Destroy(const Arena& arena)
	{
		if (arena.next)
			Destroy(*arena.next);
		arena.info.free(arena.memory);
	}

	void* Arena::Alloc(const uint32_t size, const AllocType type)
	{
		void* ptr = nullptr;
		ArenaAllocMetaData* metaData = nullptr;

		switch (type)
		{
		case AllocType::linear:
			if(front + size + sizeof(ArenaAllocMetaData) > info.chunkSize - sizeof(Arena))
			{
				if (!next)
				{
					const auto nextPtr = &static_cast<char*>(memory)[info.chunkSize - sizeof(Arena)];
					next = reinterpret_cast<Arena*>(nextPtr);
					ArenaCreateInfo createInfo = info;
					createInfo.chunkSize = Max<uint32_t>(createInfo.chunkSize, size + sizeof(ArenaAllocMetaData) + sizeof(Arena));
					*next = Create(createInfo);
				}
				return next->Alloc(size, type);
			}
			ptr = &static_cast<char*>(memory)[front];
			front += size + sizeof(ArenaAllocMetaData);
			metaData = reinterpret_cast<ArenaAllocMetaData*>(&static_cast<char*>(memory)[front - sizeof(ArenaAllocMetaData)]);
			*metaData = ArenaAllocMetaData();
			metaData->size = size;
			break;
		default:
			std::cerr << "Allocation type not implemented." << std::endl;
		}

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
