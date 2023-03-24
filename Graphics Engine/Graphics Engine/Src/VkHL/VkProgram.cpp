#include "pch.h"
#include "VkHL/VkProgram.h"

#include "JLib/Array.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"

namespace jv::vk
{
	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}

	void Free(void* ptr)
	{
		return free(ptr);
	}

	void Run(const ProgramInfo& programInfo)
	{
		const auto arenaMem = malloc(programInfo.arenaSize);
		const auto tempArenaMem = malloc(programInfo.tempArenaSize);
		const auto frameArenaMem = malloc(programInfo.frameArenaSize);
		const auto vkArenaMem = malloc(programInfo.vkArenaSize);

		Program program{};

		ArenaCreateInfo info{};
		info.alloc = Alloc;
		info.free = Free;
		info.memory = arenaMem;
		info.memorySize = programInfo.arenaSize;
		program.arena = Arena::Create(info);

		info.memory = tempArenaMem;
		info.memorySize = programInfo.tempArenaSize;
		program.tempArena = Arena::Create(info);

		info.memory = tempArenaMem;
		info.memorySize = programInfo.tempArenaSize;
		program.frameArena = Arena::Create(info);

		info.memory = vkArenaMem;
		info.memorySize = programInfo.vkArenaSize;
		program.vkCPUArena = Arena::Create(info);

		auto glfwApp = GLFWApp::Create(programInfo.name, programInfo.resolution, programInfo.fullscreen);

		Array<const char*> extensions{};
		extensions.ptr = GLFWApp::GetRequiredExtensions(extensions.length);

		init::Info vkInfo{};
		vkInfo.tempArena = &program.tempArena;
		vkInfo.createSurface = GLFWApp::CreateSurface;
		vkInfo.userPtr = &glfwApp;
		vkInfo.instanceExtensions = extensions;
		program.vkApp = CreateApp(vkInfo);

		auto swapChain = SwapChain::Create(program.arena, program.tempArena, program.vkApp, programInfo.resolution);
		program.vkGPUArena = FreeArena::Create(program.vkCPUArena, program.vkApp);

		void* userPtr = nullptr;
		if (programInfo.onBegin)
			userPtr = programInfo.onBegin(program);

		while (glfwApp.BeginFrame())
		{
			bool quit = false;

			if (programInfo.onUpdate)
				quit = !programInfo.onUpdate(program, userPtr);
			if (quit)
				break;

			swapChain.WaitForImage(program.vkApp);

			if(programInfo.onRenderUpdate)
				quit = !programInfo.onRenderUpdate(program, userPtr);
			if (quit)
				break;

			auto cmdBuffer = swapChain.BeginFrame(program.vkApp, true);

			if (programInfo.onSwapChainRenderUpdate)
				quit = !programInfo.onSwapChainRenderUpdate(program, userPtr, cmdBuffer);
			if (quit)
				break;

			swapChain.EndFrame(program.tempArena, program.vkApp);
			program.frameArena.Clear();
		}

		if (programInfo.onExit)
			programInfo.onExit(program, userPtr);

		FreeArena::Destroy(program.vkCPUArena, program.vkApp, program.vkGPUArena);
		SwapChain::Destroy(program.arena, program.vkApp, swapChain);
		init::DestroyApp(program.vkApp);

		GLFWApp::Destroy(glfwApp);
		Arena::Destroy(program.vkCPUArena);
		Arena::Destroy(program.frameArena);
		Arena::Destroy(program.tempArena);
		Arena::Destroy(program.arena);

		free(vkArenaMem);
		free(frameArenaMem);
		free(tempArenaMem);
		free(arenaMem);
	}
}
