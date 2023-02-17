#pragma once
#include <cstdint>

namespace jv
{
	template <typename T>
	struct LinkedListNode;

	template <typename T>
	struct LinkedListIterator final
	{
		LinkedListNode<T>* ptr = nullptr;

		T& operator*() const;
		T& operator->() const;

		const LinkedListIterator<T>& operator++();
		LinkedListIterator<T> operator++(int);

		friend bool operator==(const LinkedListIterator& a, const LinkedListIterator& b)
		{
			return a.ptr == b.ptr;
		}

		friend bool operator!= (const LinkedListIterator& a, const LinkedListIterator& b)
		{
			return !(a == b);
		}
	};

	template <typename T>
	struct LinkedListNode
	{
		T value{};
		LinkedListNode<T>* next = nullptr;
	};

	template <typename T>
	struct LinkedList final
	{
		LinkedListNode<T>* values = nullptr;
		
		[[nodiscard]] T& operator[](uint32_t i) const;
		[[nodiscard]] LinkedListIterator<T> begin() const;
		[[nodiscard]] static LinkedListIterator<T> end();

		T& Add(LinkedListNode<T>& node);
		void Pop();

		[[nodiscard]] uint32_t GetCount() const;
	};

	template <typename T>
	T& LinkedListIterator<T>::operator*() const
	{
		assert(ptr);
		return ptr->value;
	}

	template <typename T>
	T& LinkedListIterator<T>::operator->() const
	{
		assert(ptr);
		return ptr->value;
	}

	template <typename T>
	const LinkedListIterator<T>& LinkedListIterator<T>::operator++()
	{
		ptr = ptr->next;
		return *this;
	}

	template <typename T>
	LinkedListIterator<T> LinkedListIterator<T>::operator++(int)
	{
		Iterator temp(ptr);
		ptr = ptr->next;
		return temp;
	}

	template <typename T>
	T& LinkedList<T>::operator[](const uint32_t i) const
	{
		LinkedListNode<T>* current = values;
		for (uint32_t j = 0; j < i; ++j)
		{
			assert(current);
			current = current->next;
		}

		assert(current);
		return current->value;
	}

	template <typename T>
	LinkedListIterator<T> LinkedList<T>::begin() const
	{
		LinkedListIterator<T> it{};
		it.ptr = values;
		return it;
	}

	template <typename T>
	LinkedListIterator<T> LinkedList<T>::end()
	{
		return {};
	}

	template <typename T>
	T& LinkedList<T>::Add(LinkedListNode<T>& node)
	{
		node.next = values;
		values = node;
		return node.value;
	}

	template <typename T>
	void LinkedList<T>::Pop()
	{
		values = values->next;
	}

	template <typename T>
	uint32_t LinkedList<T>::GetCount() const
	{
		uint32_t count = 0;
		LinkedListNode<T>* current = values;
		while(current)
		{
			++count;
			current = current->next;
		}
		return count;
	}
}
