#include "pch.h"
#include <iostream>

#include "JLib/Arena.h"
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

struct ExampleApp final
{
	GLFWwindow* window = nullptr;

	bool BeginFrame() const;

	static ExampleApp Create();
	static void Destroy(const ExampleApp& app);
	static VkSurfaceKHR CreateSurface(VkInstance instance, void* userPtr);
	static const char** GetRequiredExtensions(uint32_t& count);
};

bool ExampleApp::BeginFrame() const
{
	// Check for events.
	glfwPollEvents();

	// Check if the user pressed the close button.
	if (glfwWindowShouldClose(window))
		return false;

	int32_t width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	return true;
}

ExampleApp ExampleApp::Create()
{
	ExampleApp app{};

	const int result = glfwInit();
	assert(result);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, false);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	// Create window.
	auto& window = app.window;
	window = glfwCreateWindow(800, 600, "Example App", nullptr, nullptr);
	assert(window);
	
	glfwMakeContextCurrent(window);
	return app;
}

void ExampleApp::Destroy(const ExampleApp& app)
{
	glfwDestroyWindow(app.window);
	glfwTerminate();
}

VkSurfaceKHR ExampleApp::CreateSurface(const VkInstance instance, void* userPtr)
{
	const auto self = static_cast<ExampleApp*>(userPtr);
	VkSurfaceKHR surface;
	const auto result = glfwCreateWindowSurface(instance, self->window, nullptr, &surface);
	assert(!result);
	return surface;
}

const char** ExampleApp::GetRequiredExtensions(uint32_t& count)
{
	const auto buffer = glfwGetRequiredInstanceExtensions(&count);
	return buffer;
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

	auto app = ExampleApp::Create();

	jv::Array<const char*> extensions{};
	extensions.ptr = ExampleApp::GetRequiredExtensions(extensions.length);

	jv::vk::init::Info vkInfo{};
	vkInfo.tempArena = &arena;
	vkInfo.createSurface = ExampleApp::CreateSurface;
	vkInfo.userPtr = &app;
	vkInfo.instanceExtensions = extensions;
	const auto vkApp = CreateApp(vkInfo);

	auto swapChain = jv::vk::SwapChain::Create(arena, tempArena, vkApp, glm::ivec2(800, 600));

	while(app.BeginFrame())
	{
		auto cmdBuffer = swapChain.BeginFrame(vkApp);
		swapChain.EndFrame(tempArena, vkApp);
	}

	jv::vk::SwapChain::Destroy(arena, vkApp, swapChain);
	jv::vk::init::DestroyApp(vkApp);

	ExampleApp::Destroy(app);
	jv::Arena::Destroy(arena);
}
