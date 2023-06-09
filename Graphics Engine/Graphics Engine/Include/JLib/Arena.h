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

	// Handles manual memory allocation.
	// Allocates one or more large chunks of memory which can then be (re)used for smaller linear allocations.
	struct Arena final
	{
		struct Scope final
		{
			struct Unpacked final
			{
				uint32_t depth;
				uint32_t front;
			};

			union
			{
				uint64_t handle;
				Unpacked unpacked;
			};
		};

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

		// A scope can be used to instantly delete everything that was made after the scope's creation.
		[[nodiscard]] uint64_t CreateScope() const;
		void DestroyScope(uint64_t handle);
	};

	template <typename T>
	T* Arena::New(const size_t count)
	{
		void* ptr = Alloc(sizeof(T) * count);
		T* ptrType = static_cast<T*>(ptr);
		for (uint32_t i = 0; i < count; ++i)
			new(&ptrType[i]) T();
		return ptrType;
	}
}
