#include "pch.h"
#include "VkHL/VkGLFWApp.h"

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

		return true;
	}

	GLFWApp GLFWApp::Create(const char* name, const glm::ivec2 resolution)
	{
		GLFWApp app{};

		const int result = glfwInit();
		assert(result);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, false);

		// Create window.
		auto& window = app.window;
		window = glfwCreateWindow(resolution.x, resolution.y, name, nullptr, nullptr);
		assert(window);

		glfwMakeContextCurrent(window);
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
}
