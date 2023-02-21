#include "pch.h"
#include <iostream>

#include "JLib/Arena.h"
#include "Vk/VkInit.h"

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

	static ExampleApp Create();
	static void Destroy(const ExampleApp& app);
	static VkSurfaceKHR CreateSurface(VkInstance instance, void* userPtr);
	static const char** GetRequiredExtensions(uint32_t& count);
};

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

	jv::ArenaCreateInfo info{};
	info.alloc = Alloc;
	info.free = Free;
	info.memory = c;
	info.memorySize = sizeof c;
	auto arena = jv::Arena::Create(info);

	auto app = ExampleApp::Create();

	jv::Array<const char*> extensions{};
	extensions.ptr = ExampleApp::GetRequiredExtensions(extensions.length);

	jv::vk::init::Info vkInfo{};
	vkInfo.tempArena = &arena;
	vkInfo.createSurface = ExampleApp::CreateSurface;
	vkInfo.userPtr = &app;
	vkInfo.instanceExtensions = extensions;
	const auto vkApp = CreateApp(vkInfo);

	jv::vk::init::DestroyApp(vkApp);

	ExampleApp::Destroy(app);
	jv::Arena::Destroy(arena);
}
