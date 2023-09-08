#pragma once
#include <cstdint>
#include <cstdlib>

template <typename T>
void Shuffle(T* deck, const uint32_t length)
{
	for (uint32_t i = 0; i < length; ++i)
	{
		T temp = deck[i];
		const uint32_t other = rand() % length;
		deck[i] = deck[other];
		deck[other] = temp;
	}
}
