#pragma once
#include <cstdint>

namespace jv
{
	// Linear data container that can be resized.
	template <typename T>
	struct Vector final
	{
		T* ptr = nullptr;
		uint32_t length = 0;
		uint32_t count = 0;

		[[nodiscard]] T& operator[](uint32_t i) const;
		[[nodiscard]] Iterator<T> begin() const;
		[[nodiscard]] Iterator<T> end() const;

		T& Add();
		void RemoveAt(uint32_t i);
		[[nodiscard]] T& Peek() const;
		T Pop();
	};

	template <typename T>
	T& Vector<T>::operator[](const uint32_t i) const
	{
		assert(i < count);
		return ptr[i];
	}

	template <typename T>
	Iterator<T> Vector<T>::begin() const
	{
		Iterator<T> it{};
		it.length = count;
		it.ptr = ptr;
		return it;
	}

	template <typename T>
	Iterator<T> Vector<T>::end() const
	{
		Iterator<T> it{};
		it.length = count;
		it.index = count;
		it.ptr = ptr;
		return it;
	}

	template <typename T>
	T& Vector<T>::Add()
	{
		assert(count < length);
		return ptr[count++];
	}

	template <typename T>
	void Vector<T>::RemoveAt(const uint32_t i)
	{
		assert(count > i);
		ptr[i] = ptr[--count];
	}

	template <typename T>
	T& Vector<T>::Peek() const
	{
		return ptr[count - 1];
	}

	template <typename T>
	T Vector<T>::Pop()
	{
		assert(count > 0);
		return ptr[--count];
	}
}
