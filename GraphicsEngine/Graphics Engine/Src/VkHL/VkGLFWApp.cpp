#include "pch.h"
#include "VkHL/VkGLFWApp.h"
#include <stb_image.h>

namespace jv::vk
{
	bool GLFWApp::BeginFrame() const
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

		if(onBeginFrame)
			return onBeginFrame(*window);
		return true;
	}

	void GLFWApp::Resize(const glm::ivec2 resolution, const bool fullScreen) const
	{
		const auto monitor = glfwGetPrimaryMonitor();
		const auto mode = glfwGetVideoMode(monitor);
		glfwSetWindowMonitor(window, fullScreen ? monitor : nullptr, 100, 100, resolution.x, resolution.y, mode->refreshRate);
		glfwMakeContextCurrent(window);
	}

	GLFWApp GLFWApp::Create(const char* name, const glm::ivec2 resolution, const bool fullScreen, const char* icon)
	{
		GLFWApp app{};

		const int result = glfwInit();
		assert(result);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, false);

		const auto monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);

		const auto res = fullScreen ? glm::ivec2(mode->width, mode->height) : resolution;

		// Create window.
		auto& window = app.window;
		window = glfwCreateWindow(fullScreen ? mode->width : res.x,
			fullScreen ? mode->height : res.y, name,
			fullScreen ? monitor : nullptr, nullptr);
		assert(window);

		GLFWimage images[1];
		images[0].pixels = stbi_load(icon, &images[0].width, &images[0].height, 0, 4);
		assert(images[0].pixels);
		glfwSetWindowIcon(window, 1, images);
		stbi_image_free(images[0].pixels);

		glfwMakeContextCurrent(window);

		app.fullScreen = fullScreen;
		app.resolution = resolution;
		return app;
	}

	void GLFWApp::Destroy(const GLFWApp& app)
	{
		glfwDestroyWindow(app.window);
		glfwTerminate();
	}

	VkSurfaceKHR GLFWApp::CreateSurface(const VkInstance instance, void* userPtr)
	{
		const auto self = static_cast<GLFWApp*>(userPtr);
		VkSurfaceKHR surface;
		const auto result = glfwCreateWindowSurface(instance, self->window, nullptr, &surface);
		assert(!result);
		return surface;
	}

	const char** GLFWApp::GetRequiredExtensions(uint32_t& count)
	{
		const auto buffer = glfwGetRequiredInstanceExtensions(&count);
		return buffer;
	}

	glm::ivec2 GLFWApp::GetScreenSize()
	{
		const auto monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		return { mode->width, mode->height };
	}
}
