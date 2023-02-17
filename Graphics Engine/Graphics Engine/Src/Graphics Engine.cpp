#include "pch.h"
#include <iostream>

#include "JLib/Arena.h"
#include "JLib/ArrayUtils.h"
#include "JLib/QueueUtils.h"
#include "JLib/VectorUtils.h"

void* Alloc(const uint32_t size)
{
	return malloc(size);
}

void Free(void* ptr)
{
	return free(ptr);
}

int main()
{
	char c[800];

	jv::ArenaCreateInfo info{};
	info.alloc = Alloc;
	info.free = Free;
	info.memory = c;
	info.memorySize = sizeof c;
	auto arena = jv::Arena::Create(info);
	
	for (uint32_t i = 0; i < 13; ++i)
	{
		void* p = arena.Alloc(sizeof(uint32_t));
		std::cout << arena.front << std::endl;
	}

	const auto scope = jv::ArenaScope::Create(arena);

	for (uint32_t i = 0; i < 4; ++i)
	{
		void* p = arena.Alloc(sizeof(uint32_t));
		std::cout << arena.front << std::endl;
	}

	jv::ArenaScope::Destroy(scope);

	for (uint32_t i = 0; i < 4; ++i)
	{
		void* p = arena.Alloc(sizeof(uint32_t));
		std::cout << arena.front << std::endl;
	}

	const auto arr = jv::CreateArray<float>(arena, 4);
	arr[0] = 8;
	arr[1] = 4;
	arr[2] = 6;
	for (const auto& arr1 : arr)
	{
		std::cout << arr1 << std::endl;
	}

	auto v = jv::CreateVector<int>(arena, 12);
	auto q = jv::CreateQueue<float>(arena, 8);

	std::cout << "Hello World!\n";

	for (int i = 0; i < 4; ++i)
	{
		q.Add() = i;
	}

	for (int i = 0; i < 2; ++i)
		std::cout << q.Pop() << std::endl;

	for (int i = 0; i < 4; ++i)
	{
		q.Add() = i;
	}

	for (int i = 0; i < 6; ++i)
		std::cout << q.Pop() << std::endl;

    std::cout << "Hello World!\n";
}
