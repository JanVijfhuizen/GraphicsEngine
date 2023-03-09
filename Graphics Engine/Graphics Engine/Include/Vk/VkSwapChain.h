#pragma once
#include "JLib/Array.h"

namespace jv::vk
{
	struct App;

	struct SwapChain final
	{
		struct Image final
		{
			VkImage image;
			VkImageView view;
			VkCommandBuffer cmdBuffer;
			VkFramebuffer frameBuffer;
			VkFence fence = VK_NULL_HANDLE;
		};

		struct Frame final
		{
			VkSemaphore imageAvailableSemaphore;
			VkSemaphore renderFinishedSemaphore;
			VkFence inFlightFence;
		};

		VkSurfaceFormatKHR surfaceFormat;
		VkPresentModeKHR presentMode;
		Array<Image> images;
		Array<Frame> frames;

		VkExtent2D extent;
		VkSwapchainKHR swapChain;
		VkRenderPass renderPass;

		uint32_t frameIndex = 0;
		uint32_t imageIndex;

		// Recreate resolution dependent components of the swap chain.
		void Recreate(Arena& tempArena, const App& app, glm::ivec2 resolution);

		// Wait until an image is available to draw to.
		void WaitForImage(const App& app);
		// Call this at the start of the frame.
		[[nodiscard]] VkCommandBuffer BeginFrame(const App& app, bool manuallyCallWaitForImage = false);
		// Call this at the end of the frame, after you've drawn everything.
		void EndFrame(Arena& tempArena, const App& app, const Array<VkSemaphore>& waitSemaphores = {});

		[[nodiscard]] uint32_t GetLength() const;
		[[nodiscard]] glm::ivec2 GetResolution() const;
		[[nodiscard]] VkCommandBuffer GetCmdBuffer() const;
		[[nodiscard]] uint32_t GetIndex() const;

		static SwapChain Create(Arena& arena, Arena& tempArena, const App& app, glm::ivec2 resolution);
		static void Destroy(Arena& arena, const App& app, const SwapChain& swapChain);
	};
}
