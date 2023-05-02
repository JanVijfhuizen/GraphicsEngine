#pragma once
#include "LinkedList.h"
#include "Arena.h"

namespace jv
{
	template <typename T>
	T& Add(Arena& arena, LinkedList<T>& linkedList)
	{
		auto n = arena.New<LinkedListNode<T>>();
		auto n2 = linkedList.values;
		n->next = n2;
		linkedList.values = n;
		return n->value;
	}

	template <typename T>
	T& Insert(Arena& arena, LinkedList<T>& linkedList, const uint32_t index)
	{
		const uint32_t c = linkedList.GetCount() - index;
		Add(arena, linkedList);
		LinkedListNode<T>* n = linkedList.values;

		for (uint32_t i = 0; i < c; ++i)
		{
			assert(n->next);
			n->value = n->next->value;
			n = n->next;
		}

		return n->value;
	}

	template <typename T>
	void Erase(Arena& arena, LinkedList<T>& linkedList, const uint32_t index)
	{
		LinkedListNode<T>* n = linkedList.values;
		const uint32_t c = linkedList.GetCount() - index;
		assert(n);
		
		T v{};
		for (uint32_t i = 0; i < c; ++i)
		{
			assert(n->next);
			T t = n->value;
			n->value = v;
			v = t;
			n = n->next;
		}

		Pop(arena, linkedList);
	}

	template <typename T>
	T Pop(Arena& arena, LinkedList<T>& linkedList)
	{
		assert(linkedList.values);

		LinkedListNode<T>* n = linkedList.values;
		T t = n->value;
		linkedList.values = n->next;
		arena.Free(n);
		return t;
	}

	template <typename T>
	void DestroyLinkedList(Arena& arena, const LinkedList<T>& linkedList)
	{
		while (linkedList.values)
			Pop(arena, linkedList);
	}

	template <typename T>
	Array<T> ToArray(Arena& arena, const LinkedList<T>& linkedList)
	{
		const auto arr = CreateArray<T>(arena, linkedList.GetCount());
		uint32_t i = arr.length;
		for (auto& t : linkedList)
			arr[--i] = t;
		return arr;
	}
}