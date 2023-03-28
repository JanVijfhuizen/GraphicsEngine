#include "pch.h"
#include "GE/GraphicsEngine.h"

#include "JLib/Array.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"

namespace jv::ge
{
	constexpr uint32_t ARENA_SIZE = 4096;

	struct GraphicsEngine final
	{
		bool initialized = false;

		Arena arena;
		void* arenaMem;
		Arena tempArena;
		void* tempArenaMem;

		vk::GLFWApp glfwApp;
		vk::App app;
		vk::SwapChain swapChain;
	} ge{};

	void* Alloc(const uint32_t size)
	{
		return malloc(size);
	}

	void Free(void* ptr)
	{
		return free(ptr);
	}

	void Initialize(const CreateInfo& info)
	{
		assert(!ge.initialized);
		ge.initialized = true;

		ge.arenaMem = malloc(ARENA_SIZE);
		ge.tempArenaMem = malloc(ARENA_SIZE);

		ArenaCreateInfo arenaInfo{};
		arenaInfo.alloc = Alloc;
		arenaInfo.free = Free;
		arenaInfo.memory = ge.arenaMem;
		arenaInfo.memorySize = ARENA_SIZE;
		ge.arena = Arena::Create(arenaInfo);
		arenaInfo.memory = ge.tempArenaMem;
		ge.tempArena = Arena::Create(arenaInfo);

		ge.glfwApp = vk::GLFWApp::Create(info.name, info.resolution, info.fullscreen);

		Array<const char*> extensions{};
		extensions.ptr = vk::GLFWApp::GetRequiredExtensions(extensions.length);

		vk::init::Info vkInfo{};
		vkInfo.tempArena = &ge.tempArena;
		vkInfo.createSurface = vk::GLFWApp::CreateSurface;
		vkInfo.userPtr = &ge.glfwApp;
		vkInfo.instanceExtensions = extensions;
		ge.app = CreateApp(vkInfo);

		ge.swapChain = vk::SwapChain::Create(ge.arena, ge.tempArena, ge.app, info.resolution);
	}

	void Resize(const glm::ivec2 resolution, const bool fullScreen)
	{
		assert(ge.initialized);

		ge.glfwApp.Resize(resolution, fullScreen);
		ge.swapChain.Recreate(ge.tempArena, ge.app, resolution);
	}

	bool RenderFrame()
	{
		assert(ge.initialized);

		if (!ge.glfwApp.BeginFrame())
			return false;

		ge.swapChain.WaitForImage(ge.app);
		auto cmdBuffer = ge.swapChain.BeginFrame(ge.app, true);
		ge.swapChain.EndFrame(ge.tempArena, ge.app);

		return true;
	}

	void Shutdown()
	{
		assert(ge.initialized);

		const auto result = vkDeviceWaitIdle(ge.app.device);
		assert(!result);

		vk::SwapChain::Destroy(ge.arena, ge.app, ge.swapChain);
		vk::init::DestroyApp(ge.app);
		vk::GLFWApp::Destroy(ge.glfwApp);
		Arena::Destroy(ge.tempArena);
		Arena::Destroy(ge.arena);
		free(ge.tempArenaMem);
		free(ge.arenaMem);
	}
}
