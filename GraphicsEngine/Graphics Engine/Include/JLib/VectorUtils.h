#pragma once
#include "Vector.h"

namespace jv
{
	template <typename T>
	Vector<T> CreateVector(Arena& arena, const uint32_t length)
	{
		Vector<T> vector{};
		vector.ptr = arena.New<T>(length);
		vector.length = length;
		return vector;
	}

	template <typename T>
	void DestroyVector(Arena& arena, const Vector<T>& vector)
	{
		arena.Free(vector.ptr);
	}
}
