#include "pch.h"
#include "JLib/Arena.h"
#include "JLib/JvMath.h"

namespace jv
{
	ArenaScope ArenaScope::Create(Arena& arena)
	{
		Arena* current = &arena;
		while (current->next && current->next->front > 0)
			current = current->next;

		ArenaScope scope{};
		scope.arena = current;
		scope.front = current->front;
		return scope;
	}

	void ArenaScope::Destroy(const ArenaScope& scope)
	{
		assert(scope.arena->front >= scope.front);
		scope.arena->front = scope.front;
		if (scope.arena->next)
			scope.arena->next->Clear();
	}

	Arena Arena::Create(const ArenaCreateInfo& info)
	{
		assert(info.memorySize > sizeof(Arena) + sizeof(ArenaAllocMetaData));
		assert(info.alloc);
		assert(info.free);

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
		if (next && next->front > 0)
			return next->Alloc(size);

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

#ifdef _DEBUG
		const Arena* current = next;
		while (current)
		{
			assert(current->front == 0);
			current = current->next;
		}
#endif

		return ptr;
	}

	void Arena::Free(const void* ptr)
	{
		const Arena* current = this;
		while(current)
		{
			if (current->front == 0)
				break;

			const auto metaData = reinterpret_cast<ArenaAllocMetaData*>(&static_cast<char*>(memory)[front - sizeof(ArenaAllocMetaData)]);
			const auto frontPtr = &static_cast<char*>(memory)[front - sizeof(ArenaAllocMetaData) - metaData->size];

			if (frontPtr == ptr)
			{
				front -= metaData->size + sizeof(ArenaAllocMetaData);

#ifdef _DEBUG
				current = current->next;
				while(current)
				{
					assert(current->front == 0);
					current = current->next;
				}
#endif
				return;
			}
			
			current = current->next;
		}

		throw std::exception("Pointer not contained in this arena.");
	}

	void Arena::Clear()
	{
		front = 0;
		if (next)
			next->Clear();
	}
}
