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
	jv::ArenaCreateInfo info{};
	info.alloc = Alloc;
	info.free = Free;
	info.chunkSize = sizeof(uint32_t) * 6 + sizeof(jv::Arena);
	auto arena = jv::Arena::Create(info);
	
	for (uint32_t i = 0; i < 13; ++i)
	{
		void* p = arena.Alloc(sizeof(uint32_t), jv::Arena::AllocType::linear);
		std::cout << arena.front << std::endl;
	}

    std::cout << "Hello World!\n";
}
