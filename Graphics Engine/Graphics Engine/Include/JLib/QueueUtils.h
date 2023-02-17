#pragma once
#include "Queue.h"

namespace jv
{
	template <typename T>
	Queue<T> CreateQueue(Arena& arena, const uint32_t length)
	{
		Queue<T> queue{};
		queue.ptr = arena.New<T>(length);
		queue.length = length;
		return queue;
	}

	template <typename T>
	void Destroy(Arena& arena, const Queue<T>& queue)
	{
		arena.Free(queue.ptr);
	}
}
