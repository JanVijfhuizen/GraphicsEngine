#include "pch.h"
#include "GE/GraphicsEngine.h"

#include "JLib/Array.h"
#include "JLib/LinkedList.h"
#include "JLib/LinkedListUtils.h"
#include "Vk/VkFreeArena.h"
#include "Vk/VkImage.h"
#include "Vk/VkInit.h"
#include "Vk/VkSwapChain.h"
#include "VkHL/VkGLFWApp.h"
#include "VkHL/VkMesh.h"

namespace jv::ge
{
	constexpr uint32_t ARENA_SIZE = 4096;

	struct Image final
	{
		vk::Image image;
		ImageCreateInfo info;
	};

	struct Mesh final
	{
		vk::Mesh mesh;
		MeshCreateInfo info;
	};

	struct Scene final
	{
		Arena arena;
		void* arenaMem;
		vk::FreeArena freeArena;
		LinkedList<Image> images{};
		LinkedList<Mesh> meshes{};
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

		auto& scene = Add(ge.arena, ge.scenes) = {};
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

	uint32_t AddImage(const ImageCreateInfo& info, const uint32_t handle)
	{
		assert(ge.initialized);
		auto& scene = ge.scenes[handle];

		vk::ImageCreateInfo vkImageCreateInfo{};
		glm::vec3 resolution = glm::vec3(info.resolution, 1);

		switch (info.format)
		{
			case ImageCreateInfo::Format::color:
				vkImageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				resolution.z = 3;
				break;
			case ImageCreateInfo::Format::grayScale:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				break;
			case ImageCreateInfo::Format::depth:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				break;
			case ImageCreateInfo::Format::stencil:
				vkImageCreateInfo.format = VK_FORMAT_R8_UNORM;
				vkImageCreateInfo.aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
				vkImageCreateInfo.usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				break;
			default:
				std::cerr << "Format not supported." << std::endl;
		}

		const auto vkImage = vk::Image::Create(scene.arena, scene.freeArena, ge.app, vkImageCreateInfo, resolution);
		auto& image = Add(scene.arena, scene.images) = {};
		image.image = vkImage;
		image.info = info;
		return scene.images.GetCount() - 1;
	}

	void FillImage(const uint32_t sceneHandle, const uint32_t imageHandle, unsigned char* pixels)
	{
		assert(ge.initialized);
		auto& scene = ge.scenes[sceneHandle];
		auto& image = scene.images[imageHandle];
		image.image.FillImage(scene.arena, scene.freeArena, ge.app, pixels);
	}

	uint32_t AddMesh(const MeshCreateInfo& info, const uint32_t handle)
	{
		assert(ge.initialized);
		auto& scene = ge.scenes[handle];

		Array<uint16_t> indices{};
		indices.ptr = info.indices;
		indices.length = info.indicesLength;

		vk::Mesh vkMesh{};
		if(info.type == MeshCreateInfo::Type::vertex2D)
		{
			Array<vk::Vertex2d> v2d{};
			v2d.length = info.verticesLength;
			v2d.ptr = reinterpret_cast<vk::Vertex2d*>(info.vertices2d);
			vkMesh = vk::Mesh::Create(scene.arena, scene.freeArena, ge.app, v2d, indices);
		}
		else if(info.type == MeshCreateInfo::Type::vertex3D)
		{
			Array<vk::Vertex3d> v3d{};
			v3d.length = info.verticesLength;
			v3d.ptr = reinterpret_cast<vk::Vertex3d*>(info.vertices3d);
			vkMesh = vk::Mesh::Create(scene.arena, scene.freeArena, ge.app, v3d, indices);
		}
		else
			std::cerr << "Vertex type not supported." << std::endl;

		auto& mesh = Add(scene.arena, scene.meshes) = {};
		mesh.mesh = vkMesh;
		mesh.info = info;
			
		return scene.meshes.GetCount() - 1;
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

	uint32_t GetFrameCount()
	{
		assert(ge.initialized);
		return ge.swapChain.GetLength();
	}

	uint32_t GetFrameIndex()
	{
		assert(ge.initialized);
		return ge.swapChain.GetIndex();
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
