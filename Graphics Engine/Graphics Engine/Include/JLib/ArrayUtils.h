#pragma once
#include "Array.h"

namespace jv
{
	template <typename T>
	Array<T> CreateArray(Arena& arena, const uint32_t length)
	{
		Array<T> array{};
		array.ptr = arena.New<T>(length);
		array.length = length;
		return array;
	}

	template <typename T>
	void Destroy(Arena& arena, const Array<T>& array)
	{
		arena.Free(array.ptr);
	}
}
