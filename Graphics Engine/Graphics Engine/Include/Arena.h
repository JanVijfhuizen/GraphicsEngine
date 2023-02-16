#pragma once

namespace jv
{
	struct ArenaCreateInfo final
	{
		void* (*alloc)(uint32_t size) = nullptr;
		void (*free)(void* ptr) = nullptr;
		uint32_t chunkSize = 4096;
	};

	struct ArenaAllocMetaData final
	{
		uint32_t size;
	};

	struct Arena final
	{
		enum class AllocType
		{
			free,
			linear
		};

		ArenaCreateInfo info;
		void* memory;
		uint32_t front = 0;
		Arena* next = nullptr;

		[[nodiscard]] static Arena Create(const ArenaCreateInfo& info);
		static void Destroy(const Arena& arena);

		void* Alloc(uint32_t size, AllocType type);
		void Free(const void* ptr);
	};
}
