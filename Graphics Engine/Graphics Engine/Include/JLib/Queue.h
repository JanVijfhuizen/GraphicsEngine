#pragma once
#include <cstdint>

namespace jv
{
	template <typename T>
	struct Queue;

	template <typename T>
	struct QueueIterator final
	{
		Queue<T>* ptr = nullptr;
		uint32_t index = 0;

		T& operator*() const;
		T& operator->() const;

		const QueueIterator& operator++();
		QueueIterator operator++(int);

		friend bool operator==(const QueueIterator& a, const QueueIterator& b)
		{
			return a.index == b.index;
		}

		friend bool operator!= (const QueueIterator& a, const QueueIterator& b)
		{
			return !(a == b);
		}
	};

	template <typename T>
	struct Queue final
	{
		T* ptr = nullptr;
		uint32_t length = 0;
		uint32_t count = 0;
		uint32_t front = 0;

		[[nodiscard]] T& operator[](uint32_t i) const;
		[[nodiscard]] QueueIterator<T> begin() const;
		[[nodiscard]] QueueIterator<T> end() const;

		T& Add();
		[[nodiscard]] T& Peek() const;
		T Pop();

		[[nodiscard]] uint32_t GetIndex(uint32_t i) const;
	};

	template <typename T>
	T& QueueIterator<T>::operator*() const
	{
		assert(ptr);
		assert(index <= ptr->length);
		return ptr[index];
	}

	template <typename T>
	T& QueueIterator<T>::operator->() const
	{
		assert(ptr);
		assert(index <= ptr->length);
		return ptr[index];
	}

	template <typename T>
	const QueueIterator<T>& QueueIterator<T>::operator++()
	{
		++index;
		return *this;
	}

	template <typename T>
	QueueIterator<T> QueueIterator<T>::operator++(int)
	{
		Iterator temp(ptr, ptr->length, index);
		++index;
		return temp;
	}

	template <typename T>
	T& Queue<T>::operator[](const uint32_t i) const
	{
		assert(i < count);
		return ptr[GetIndex(i)];
	}

	template <typename T>
	QueueIterator<T> Queue<T>::begin() const
	{
		QueueIterator<T> it{};
		it.length = count;
		it.ptr = ptr;
		return it;
	}

	template <typename T>
	QueueIterator<T> Queue<T>::end() const
	{
		QueueIterator<T> it{};
		it.length = count;
		it.index = count;
		it.ptr = ptr;
		return it;
	}

	template <typename T>
	T& Queue<T>::Add()
	{
		assert(count < length);
		return ptr[GetIndex(count++)];
	}

	template <typename T>
	T& Queue<T>::Peek() const
	{
		assert(count > 0);
		return ptr[GetIndex(count - 1)];
	}

	template <typename T>
	T Queue<T>::Pop()
	{
		assert(count > 0);
		return ptr[GetIndex(--count)];
	}

	template <typename T>
	uint32_t Queue<T>::GetIndex(const uint32_t i) const
	{
		return (i + front + length) % length;
	}
}
