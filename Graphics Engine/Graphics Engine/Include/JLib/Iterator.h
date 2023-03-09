#pragma once

namespace jv
{
	// Can be used to linearly iterate (foreach loops) over more basic data structures.
	template <typename T>
	struct Iterator final
	{
		T* ptr = nullptr;
		uint32_t length = 0;
		uint32_t index = 0;

		T& operator*() const;
		T& operator->() const;

		const Iterator& operator++();
		Iterator operator++(int);

		friend bool operator==(const Iterator& a, const Iterator& b)
		{
			return a.index == b.index;
		}

		friend bool operator!= (const Iterator& a, const Iterator& b)
		{
			return !(a == b);
		}
	};

	template <typename T>
	T& Iterator<T>::operator*() const
	{
		assert(ptr);
		assert(index <= length);
		return ptr[index];
	}

	template <typename T>
	T& Iterator<T>::operator->() const
	{
		assert(ptr);
		assert(index <= length);
		return ptr[index];
	}

	template <typename T>
	const Iterator<T>& Iterator<T>::operator++()
	{
		++index;
		return *this;
	}

	template <typename T>
	Iterator<T> Iterator<T>::operator++(int)
	{
		Iterator temp(ptr, length, index);
		++index;
		return temp;
	}
}