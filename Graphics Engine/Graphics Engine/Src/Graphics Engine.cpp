#include "pch.h"
#include <iostream>

#include "Arena.h"

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

	auto scope = jv::ArenaScope::Create(arena);

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

    std::cout << "Hello World!\n";
}
