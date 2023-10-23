#include "pch.h"
#include "JLib/Arena.h"
#include "JLib/Math.h"

namespace jv
{
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

	void* Arena::Alloc(uint32_t size)
	{
		bool goNext = false;
		auto n = next;
		while(n)
		{
			goNext = goNext ? true : n->front > 0;
			n = n->next;
		}
		if (goNext)
			return next->Alloc(size);

		size += (4 - size) % 4;

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

		throw std::exception("Pointer not in front of this arena.");
	}

	void Arena::Clear()
	{
		front = 0;
		if (next)
			next->Clear();
	}

	uint32_t Arena::GetTotalUsedMemory() const
	{
		uint32_t size = 0;

		const Arena* current = this;
		while (current)
		{
			++size;
			current = current->next;
		}
		return size * info.memorySize;
	}

	uint64_t Arena::CreateScope() const
	{
		const Arena* current = this;
		uint32_t depth = 0;
		while (current->next && current->next->front > 0)
		{
			current = current->next;
			++depth;
		}

		Scope scope{};
		scope.unpacked.depth = depth;
		scope.unpacked.front = current->front;
		return scope.handle;
	}

	void Arena::DestroyScope(const uint64_t handle)
	{
		Scope scope{};
		scope.handle = handle;

		Arena* current = this;
		for (uint32_t i = 0; i < scope.unpacked.depth; ++i)
			current = current->next;
		if (current->next)
			current->next->Clear();
		current->front = scope.unpacked.front;
	}
}
