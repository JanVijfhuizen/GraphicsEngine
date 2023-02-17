#pragma once
#include <cstdint>
#include "Iterator.h"

namespace jv
{
	template <typename T>
	struct Array final
	{
		T* ptr = nullptr;
		uint32_t length = 0;

		[[nodiscard]] T& operator[](uint32_t i) const;
		[[nodiscard]] Iterator<T> begin() const;
		[[nodiscard]] Iterator<T> end() const;
	};

	template <typename T>
	T& Array<T>::operator[](const uint32_t i) const
	{
		assert(i < length);
		assert(ptr);
		return ptr[i];
	}

	template <typename T>
	Iterator<T> Array<T>::begin() const
	{
		Iterator<T> it{};
		it.length = length;
		it.ptr = ptr;
		return it;
	}

	template <typename T>
	Iterator<T> Array<T>::end() const
	{
		Iterator<T> it{};
		it.length = length;
		it.index = length;
		it.ptr = ptr;
		return it;
	}
}
