#pragma once

namespace jv
{
	struct ArenaCreateInfo final
	{
		void* (*alloc)(uint32_t size);
		void (*free)(void* ptr);
		void* memory = nullptr;
		uint32_t memorySize = 4096;
	};

	struct ArenaAllocMetaData final
	{
		uint32_t size;
	};

	struct Arena final
	{
		ArenaCreateInfo info;
		void* memory;
		uint32_t front = 0;
		Arena* next = nullptr;

		[[nodiscard]] static Arena Create(const ArenaCreateInfo& info);
		static void Destroy(const Arena& arena);

		void* Alloc(uint32_t size);
		void Free(const void* ptr);
		void Clear();
	};
}
