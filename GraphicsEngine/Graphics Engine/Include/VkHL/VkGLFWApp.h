#pragma once

namespace jv::vk
{
	struct GLFWApp final
	{
		GLFWwindow* window;
		bool fullScreen;
		glm::ivec2 resolution;

		bool (*onBeginFrame)(GLFWwindow& window) = nullptr;

		// Returns false when the window is closed.
		[[nodiscard]] bool BeginFrame() const;

		void Resize(glm::ivec2 resolution, bool fullScreen) const;

		static GLFWApp Create(const char* name, glm::ivec2 resolution, bool fullScreen);
		static void Destroy(const GLFWApp& app);

		static VkSurfaceKHR CreateSurface(VkInstance instance, void* userPtr);
		static const char** GetRequiredExtensions(uint32_t& count);
	};
}
