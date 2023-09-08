#pragma once
#include "Heap.h"

namespace jv
{
	template <typename T>
	Heap<T> CreateHeap(Arena& arena, const uint32_t length)
	{
		Heap<T> instance{};
		instance.data = arena.New<KeyPair<T>>(length + 1);
		instance.length = length;
		return instance;
	}

	template <typename T>
	void DestroyHeap(Heap<T>& instance, Arena& arena)
	{
		arena.Free(instance.data);
	}
}
