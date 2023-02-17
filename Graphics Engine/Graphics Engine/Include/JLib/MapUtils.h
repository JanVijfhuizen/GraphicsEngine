#pragma once
#include "Map.h"

namespace jv
{
	template <typename T>
	Map<T> CreateMap(Arena& arena, const uint32_t length)
	{
		Map<T> map{};
		map.ptr = arena.New<KeyPair<T>>(length);
		map.length = length;
		for (uint32_t i = 0; i < map.length; ++i)
			map.ptr[i] = KeyPair<T>();
		return map;
	}

	template <typename T>
	void Destroy(Arena& arena, const Map<T>& map)
	{
		arena.Free(map.ptr);
	}
}
