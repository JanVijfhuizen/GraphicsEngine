#pragma once
#include "Vk/VkApp.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkSwapChain.h"

namespace jv::vk
{
	constexpr uint32_t DEFAULT_ARENA_PAGE_SIZE = 4096;

	// JV Vulkan Program.
	struct Program final
	{
		// Screen resolution.
		glm::ivec2 resolution;

		// Vulkan application.
		App vkApp;
		// Persistent allocations.
		Arena arena;
		// Temporary stack allocations.
		Arena tempArena;
		// Per-frame allocations. Cleared automatically at the end of the frame.
		Arena frameArena;
		// Vulkan CPU allocator.
		Arena vkCPUArena;
		// Vulkan GPU allocator.
		FreeArena vkGPUArena;
		// Vulkan swap chain render pass.
		VkRenderPass swapChainRenderPass;
		// Amount of frames in the swap chain.
		uint32_t frameCount;
		// Current frame in the swap chain.
		uint32_t frameIndex;
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

		// Returns a user defined pointer.
		void*(*onBegin)(Program& program) = nullptr;
		bool(*onUpdate)(Program& program, void* userPtr) = nullptr;
		bool(*onExit)(Program& program, void* userPtr) = nullptr;

		bool(*onRenderUpdate)(Program& program, void* userPtr) = nullptr;
		bool(*onSwapChainRenderUpdate)(Program& program, void* userPtr, VkCommandBuffer cmd) = nullptr;
	};

	void Run(const ProgramInfo& programInfo);
}
