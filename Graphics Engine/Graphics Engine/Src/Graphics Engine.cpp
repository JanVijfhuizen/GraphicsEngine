#include "pch.h"
#include <iostream>

#include "JLib/Arena.h"
#include "Vk/VkGLFWApp.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"

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
	char c[800]{};
	char tc[800]{};

	jv::ArenaCreateInfo info{};
	info.alloc = Alloc;
	info.free = Free;
	info.memory = c;
	info.memorySize = sizeof c;
	auto arena = jv::Arena::Create(info);

	info.memory = tc;
	info.memorySize = sizeof tc;
	auto tempArena = jv::Arena::Create(info);

	auto glfwApp = jv::vk::GLFWApp::Create("Example App", {800, 600});

	jv::Array<const char*> extensions{};
	extensions.ptr = jv::vk::GLFWApp::GetRequiredExtensions(extensions.length);

	jv::vk::init::Info vkInfo{};
	vkInfo.tempArena = &tempArena;
	vkInfo.createSurface = jv::vk::GLFWApp::CreateSurface;
	vkInfo.userPtr = &glfwApp;
	vkInfo.instanceExtensions = extensions;
	const auto vkApp = CreateApp(vkInfo);

	auto swapChain = jv::vk::SwapChain::Create(arena, tempArena, vkApp, glm::ivec2(800, 600));

	while(glfwApp.BeginFrame())
	{
		auto cmdBuffer = swapChain.BeginFrame(vkApp);
		swapChain.EndFrame(tempArena, vkApp);
	}

	jv::vk::SwapChain::Destroy(arena, vkApp, swapChain);
	jv::vk::init::DestroyApp(vkApp);

	jv::vk::GLFWApp::Destroy(glfwApp);
	jv::Arena::Destroy(tempArena);
	jv::Arena::Destroy(arena);
}
