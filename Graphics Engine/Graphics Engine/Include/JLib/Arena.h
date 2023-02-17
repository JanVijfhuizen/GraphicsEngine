#pragma once

namespace jv
{
	struct Arena;

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

	struct ArenaScope final
	{
		Arena* arena;
		uint32_t front;

		[[nodiscard]] static ArenaScope Create(Arena& arena);
		static void Destroy(const ArenaScope& scope);
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

		template <typename T>
		[[nodiscard]] T* New(size_t count = 1);
	};

	template <typename T>
	T* Arena::New(const size_t count)
	{
		void* ptr = Alloc(sizeof(T) * count);
		T* ptrType = static_cast<T*>(ptr);
		return ptrType;
	}
}
