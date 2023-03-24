#pragma once
#include "Vk/VkFreeArena.h"

namespace jv::vk
{
	constexpr uint32_t DEFAULT_ARENA_PAGE_SIZE = 4096;

	struct Program final
	{
		Arena arena;
		Arena tempArena;
		Arena frameArena;
		FreeArena freeArena;
	};

	struct ProgramInfo final
	{
		const char* name = "Default JV VK Program";
		glm::ivec2 resolution{800, 600};
		bool fullscreen = false;

		uint32_t arenaSize = DEFAULT_ARENA_PAGE_SIZE;
		uint32_t tempArenaSize = DEFAULT_ARENA_PAGE_SIZE;
		uint32_t vkArenaSize = DEFAULT_ARENA_PAGE_SIZE;
		uint32_t frameArenaSize = DEFAULT_ARENA_PAGE_SIZE;

		void*(*onBegin)(Program& program) = nullptr;
		bool(*onUpdate)(Program& program, void* userPtr) = nullptr;
		bool(*onExit)(Program& program, void* userPtr) = nullptr;

		bool(*onRenderUpdate)(Program& program) = nullptr;
		bool(*onSwapChainRenderUpdate)(Program& program, VkCommandBuffer cmd) = nullptr;
	};

	void Run(const ProgramInfo& programInfo);
}
