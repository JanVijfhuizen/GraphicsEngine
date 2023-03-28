#include "pch.h"
#include "GE/GraphicsEngine.h"

#include "JLib/Array.h"
#include "JLib/LinkedList.h"
#include "JLib/LinkedListUtils.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"

namespace jv::ge
{
	constexpr uint32_t ARENA_SIZE = 4096;

	struct Scene final
	{
		Arena arena;
		void* arenaMem;
		vk::FreeArena freeArena;
	};

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

		uint64_t scope;

		LinkedList<Scene> scenes{};
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
		ge.scope = ge.arena.CreateScope();
	}

	void Resize(const glm::ivec2 resolution, const bool fullScreen)
	{
		assert(ge.initialized);

		ge.glfwApp.Resize(resolution, fullScreen);
		ge.swapChain.Recreate(ge.tempArena, ge.app, resolution);
	}

	uint32_t CreateScene()
	{
		constexpr uint32_t SCENE_ARENA_SIZE = 512;

		assert(ge.initialized);

		auto& scene = Add(ge.arena, ge.scenes);
		scene.arenaMem = malloc(SCENE_ARENA_SIZE);

		ArenaCreateInfo arenaInfo{};
		arenaInfo.alloc = Alloc;
		arenaInfo.free = Free;
		arenaInfo.memory = scene.arenaMem;
		arenaInfo.memorySize = SCENE_ARENA_SIZE;
		scene.arena = Arena::Create(arenaInfo);
		scene.freeArena = vk::FreeArena::Create(scene.arena, ge.app);

		return ge.scenes.GetCount() - 1;
	}

	void ClearScene(const uint32_t handle)
	{
		assert(ge.initialized);

		auto& scene = ge.scenes[handle];
		assert(ge.initialized);
	}

	void DestroyScenes()
	{
		assert(ge.initialized);

		for (auto& scene : ge.scenes)
		{
			vk::FreeArena::Destroy(scene.arena, ge.app, scene.freeArena);
			Arena::Destroy(scene.arena);
			free(scene.arenaMem);
		}
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

		DestroyScenes();
		ge.arena.DestroyScope(ge.scope);
		vk::SwapChain::Destroy(ge.arena, ge.app, ge.swapChain);
		vk::init::DestroyApp(ge.app);
		vk::GLFWApp::Destroy(ge.glfwApp);
		Arena::Destroy(ge.tempArena);
		Arena::Destroy(ge.arena);
		free(ge.tempArenaMem);
		free(ge.arenaMem);

		ge.initialized = false;
	}
}
