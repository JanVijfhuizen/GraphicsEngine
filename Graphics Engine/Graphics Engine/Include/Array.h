#pragma once
#include "Arena.h"
#include <cstdint>

namespace jv
{
	template <typename T>
	struct Array final
	{
		T* ptr;
		uint32_t length;

		static Array Create(T* ptr, uint32_t length);
		static Array Create(Arena& arena, uint32_t length);
		static void Destroy(Arena& arena, const Array& array);
	};

	template <typename T>
	Array<T> Array<T>::Create(T* ptr, const uint32_t length)
	{
		Array<T> array{};
		array.ptr = ptr;
		array.length = length;
		return array;
	}

	template <typename T>
	Array<T> Array<T>::Create(Arena& arena, const uint32_t length)
	{
		
	}

	template <typename T>
	void Array<T>::Destroy(Arena& arena, const Array& array)
	{
	}
}
