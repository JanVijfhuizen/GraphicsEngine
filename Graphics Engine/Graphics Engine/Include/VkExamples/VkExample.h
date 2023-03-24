#pragma once

namespace jv::vk::example
{
	constexpr uint32_t DEFAULT_ARENA_SIZE = 4096;

	struct Info final
	{
		const char* name = "Example JV VK App";
		glm::ivec2 resolution{800, 600};
		uint32_t arenaSize = DEFAULT_ARENA_SIZE;
		uint32_t tempArenaSize = DEFAULT_ARENA_SIZE;
		uint32_t vkArenaSize = DEFAULT_ARENA_SIZE;
		uint32_t frameArenaSize = DEFAULT_ARENA_SIZE;
	};

	void Run(const Info& exampleInfo);
}
