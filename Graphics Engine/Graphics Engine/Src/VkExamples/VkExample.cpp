#include "pch.h"
#include "VkExamples/VkExample.h"

#include "JLib/Array.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"

namespace jv::vk::example
{
	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}

	void Free(void* ptr)
	{
		return free(ptr);
	}

	void Run(const Info& exampleInfo)
	{
		const auto arenaMem = malloc(exampleInfo.arenaSize);
		const auto tempArenaMem = malloc(exampleInfo.tempArenaSize);
		const auto frameArenaMem = malloc(exampleInfo.frameArenaSize);
		const auto vkArenaMem = malloc(exampleInfo.vkArenaSize);

		ArenaCreateInfo info{};
		info.alloc = Alloc;
		info.free = Free;
		info.memory = arenaMem;
		info.memorySize = exampleInfo.arenaSize;
		auto arena = Arena::Create(info);

		info.memory = tempArenaMem;
		info.memorySize = exampleInfo.tempArenaSize;
		auto tempArena = Arena::Create(info);

		info.memory = tempArenaMem;
		info.memorySize = exampleInfo.tempArenaSize;
		auto frameArena = Arena::Create(info);

		info.memory = vkArenaMem;
		info.memorySize = exampleInfo.vkArenaSize;
		auto vkArena = Arena::Create(info);

		auto glfwApp = GLFWApp::Create(exampleInfo.name, exampleInfo.resolution);

		Array<const char*> extensions{};
		extensions.ptr = GLFWApp::GetRequiredExtensions(extensions.length);

		init::Info vkInfo{};
		vkInfo.tempArena = &tempArena;
		vkInfo.createSurface = GLFWApp::CreateSurface;
		vkInfo.userPtr = &glfwApp;
		vkInfo.instanceExtensions = extensions;
		const auto vkApp = CreateApp(vkInfo);

		auto swapChain = SwapChain::Create(arena, tempArena, vkApp, exampleInfo.resolution);
		const auto freeArena = FreeArena::Create(vkArena, vkApp);

		while (glfwApp.BeginFrame())
		{
			auto cmdBuffer = swapChain.BeginFrame(vkApp);
			swapChain.EndFrame(tempArena, vkApp);
		}

		FreeArena::Destroy(vkArena, vkApp, freeArena);
		SwapChain::Destroy(arena, vkApp, swapChain);
		init::DestroyApp(vkApp);

		GLFWApp::Destroy(glfwApp);
		Arena::Destroy(vkArena);
		Arena::Destroy(frameArena);
		Arena::Destroy(tempArena);
		Arena::Destroy(arena);

		free(vkArenaMem);
		free(frameArenaMem);
		free(tempArenaMem);
		free(arenaMem);
	}
}
