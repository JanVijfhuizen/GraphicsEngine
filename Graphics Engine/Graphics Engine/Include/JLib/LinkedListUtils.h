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
	void Destroy(Arena& arena, const LinkedList<T>& linkedList)
	{
		while (linkedList.values)
			Pop(arena, linkedList);
	}
}