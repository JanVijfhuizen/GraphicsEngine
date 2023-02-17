#pragma once
#include <cstdint>
#include "Arena.h"

namespace jv
{
	template<typename T>
	struct LinkedList;

	template <typename T>
	struct LinkedListIterator final
	{
		typename LinkedList<T>::Node* ptr = nullptr;

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
	struct LinkedList final
	{
		struct Node final
		{
			T value{};
			Node* next = nullptr;
		};

		Node* values = nullptr;
		Arena* arena = nullptr;
		ArenaScope scope{};

		[[nodiscard]] static LinkedList<T> Create(Arena& arena);
		static void Destroy(const LinkedList<T>& linkedList);

		[[nodiscard]] T& operator[](uint32_t i) const;
		[[nodiscard]] LinkedListIterator<T> begin() const;
		[[nodiscard]] static LinkedListIterator<T> end();

		T& Add();
		[[nodiscard]] T& Peek() const;
		T Pop();
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
		Node* current = values;
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
	T& LinkedList<T>::Add()
	{
		Node* n = arena->New<Node>();
		Node* n2 = values;
		n->next = n2;
		values = n;
		return n->value;
	}

	template <typename T>
	T& LinkedList<T>::Peek() const
	{
		assert(values);
		return values->value;
	}

	template <typename T>
	T LinkedList<T>::Pop()
	{
		assert(values);
		assert(arena);

		Node* n = values;
		T t = n->value;
		values = n->next;
		arena->Free(n);
		return t;
	}

	template <typename T>
	uint32_t LinkedList<T>::GetCount() const
	{
		uint32_t count = 0;
		Node* current = values;
		while(current)
		{
			++count;
			current = current->next;
		}
		return count;
	}

	template <typename T>
	LinkedList<T> LinkedList<T>::Create(Arena& arena)
	{
		LinkedList<T> linkedList{};
		linkedList.arena = &arena;
		linkedList.scope = ArenaScope::Create(arena);
		return linkedList;
	}

	template <typename T>
	void LinkedList<T>::Destroy(const LinkedList<T>& linkedList)
	{
		ArenaScope::Destroy(linkedList.scope);
	}
}
