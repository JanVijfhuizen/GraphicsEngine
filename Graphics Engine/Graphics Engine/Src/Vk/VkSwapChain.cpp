#include "pch.h"
#include "Vk/VkSwapChain.h"

#include "JLib/ArrayUtils.h"
#include "JLib/Math.h"
#include "Vk/VkInit.h"

namespace jv::vk
{
	constexpr uint32_t SWAPCHAIN_MAX_FRAMES_IN_FLIGHT = 4;

	VkSurfaceFormatKHR ChooseSurfaceFormat(const Array<VkSurfaceFormatKHR>& availableFormats)
	{
		// Preferably go for SRGB, if it's not present just go with the first one found.
		// We can basically assume that SRGB is supported on most hardware.
		for (const auto& availableFormat : availableFormats)
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		return availableFormats[0];
	}

	VkPresentModeKHR ChoosePresentMode(const Array<VkPresentModeKHR>& availablePresentModes)
	{
		// Preferably go for Mailbox, otherwise go for Fifo.
		// Fifo is traditional VSync, where mailbox is all that and better, but unlike Fifo is not required to be supported by the hardware.
		for (const auto& availablePresentMode : availablePresentModes)
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, const glm::ivec2 resolution)
	{
		if (capabilities.currentExtent.width != UINT32_MAX)
			return capabilities.currentExtent;

		VkExtent2D actualExtent =
		{
			static_cast<uint32_t>(resolution.x),
			static_cast<uint32_t>(resolution.y)
		};

		const auto& minExtent = capabilities.minImageExtent;
		const auto& maxExtent = capabilities.maxImageExtent;

		actualExtent.width = Clamp<uint32_t>(actualExtent.width, minExtent.width, maxExtent.width);
		actualExtent.height = Clamp<uint32_t>(actualExtent.height, minExtent.height, maxExtent.height);

		return actualExtent;
	}

	void Cleanup(const App& app, const SwapChain& swapChain)
	{
		if (!swapChain.swapChain)
			return;

		const auto result = vkDeviceWaitIdle(app.device);
		assert(!result);

		for (auto& image : swapChain.images)
		{
			vkDestroyImageView(app.device, image.view, nullptr);
			if (image.fence)
				vkWaitForFences(app.device, 1, &image.fence, VK_TRUE, UINT64_MAX);
			image.fence = VK_NULL_HANDLE;
			vkFreeCommandBuffers(app.device, app.commandPool, 1, &image.cmdBuffer);
			vkDestroyFramebuffer(app.device, image.frameBuffer, nullptr);
		}

		for (const auto& frame : swapChain.frames)
		{
			vkDestroySemaphore(app.device, frame.imageAvailableSemaphore, nullptr);
			vkDestroySemaphore(app.device, frame.renderFinishedSemaphore, nullptr);
			vkDestroyFence(app.device, frame.inFlightFence, nullptr);
		}

		vkDestroyRenderPass(app.device, swapChain.renderPass, nullptr);
		vkDestroySwapchainKHR(app.device, swapChain.swapChain, nullptr);
	}

	void SwapChain::Recreate(Arena& tempArena, const App& app, const glm::ivec2 resolution)
	{
		const auto scope = tempArena.CreateScope();
		const auto support = init::QuerySwapChainSupport(tempArena, app.physicalDevice, app.surface);
		extent = ChooseExtent(support.capabilities, resolution);

		const auto families = init::GetQueueFamilies(tempArena, app.physicalDevice, app.surface);

		const uint32_t queueFamilyIndices[] =
		{
			static_cast<uint32_t>(families.graphics),
			static_cast<uint32_t>(families.present)
		};

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = app.surface;
		createInfo.minImageCount = static_cast<uint32_t>(images.length);
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (families.graphics != families.present)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}

		createInfo.preTransform = support.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = swapChain;

		VkSwapchainKHR newSwapChain;
		const auto result = vkCreateSwapchainKHR(app.device, &createInfo, nullptr, &newSwapChain);
		assert(!result);

		Cleanup(app, *this);
		swapChain = newSwapChain;

		auto length = images.length;
		const auto vkImages = CreateArray<VkImage>(tempArena, length);
		vkGetSwapchainImagesKHR(app.device, swapChain, &length, vkImages.ptr);

		VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
		cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufferAllocInfo.commandPool = app.commandPool;
		cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufferAllocInfo.commandBufferCount = length;

		const auto cmdBuffers = CreateArray<VkCommandBuffer>(tempArena, length);
		const auto cmdResult = vkAllocateCommandBuffers(app.device, &cmdBufferAllocInfo, cmdBuffers.ptr);
		assert(!cmdResult);

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription{};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency subpassDependency{};
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachmentInfo{};
		attachmentInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentInfo.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentInfo.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentInfo.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachmentInfo.format = surfaceFormat.format;

		VkRenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.pAttachments = &attachmentInfo;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;
		renderPassCreateInfo.dependencyCount = 1;

		const auto renderPassResult = vkCreateRenderPass(app.device, &renderPassCreateInfo, nullptr, &renderPass);
		assert(!renderPassResult);

		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.format = surfaceFormat.format;

		VkFramebufferCreateInfo frameBufferCreateInfo{};
		frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferCreateInfo.layers = 1;
		frameBufferCreateInfo.attachmentCount = 1;
		frameBufferCreateInfo.renderPass = renderPass;
		frameBufferCreateInfo.width = extent.width;
		frameBufferCreateInfo.height = extent.height;

		// Create images.
		for (uint32_t i = 0; i < length; ++i)
		{
			auto& image = images[i];
			image.image = vkImages[i];

			viewCreateInfo.image = image.image;
			const auto viewResult = vkCreateImageView(app.device, &viewCreateInfo, nullptr, &image.view);
			assert(!viewResult);

			image.cmdBuffer = cmdBuffers[i];
			frameBufferCreateInfo.pAttachments = &image.view;
			const auto frameBufferResult = vkCreateFramebuffer(app.device, &frameBufferCreateInfo, nullptr, &image.frameBuffer);
			assert(!frameBufferResult);
		}

		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceCreateInfo{};
		fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		// Create frames.
		for (auto& frame : frames)
		{
			auto semaphoreResult = vkCreateSemaphore(app.device, &semaphoreCreateInfo, nullptr, &frame.imageAvailableSemaphore);
			assert(!semaphoreResult);
			semaphoreResult = vkCreateSemaphore(app.device, &semaphoreCreateInfo, nullptr, &frame.renderFinishedSemaphore);
			assert(!semaphoreResult);

			const auto fenceResult = vkCreateFence(app.device, &fenceCreateInfo, nullptr, &frame.inFlightFence);
			assert(!fenceResult);
		}

		tempArena.DestroyScope(scope);
	}

	void SwapChain::WaitForImage(const App& app)
	{
		const auto& frame = frames[frameIndex];

		auto result = vkWaitForFences(app.device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
		assert(!result);
		result = vkAcquireNextImageKHR(app.device,
			swapChain, UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
		assert(!result);

		auto& image = images[imageIndex];
		if (image.fence)
			vkWaitForFences(app.device, 1, &image.fence, VK_TRUE, UINT64_MAX);
		image.fence = frame.inFlightFence;
	}

	VkCommandBuffer SwapChain::BeginFrame(const App& app, const bool manuallyCallWaitForImage)
	{
		if (!manuallyCallWaitForImage)
			WaitForImage(app);

		const auto& image = images[imageIndex];

		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkResetCommandBuffer(image.cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
		vkBeginCommandBuffer(image.cmdBuffer, &cmdBufferBeginInfo);

		const VkClearValue clearColor = { 1.f, 1.f, 1.f, 1.f };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = image.frameBuffer;
		renderPassBeginInfo.renderArea.extent = extent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(image.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		return image.cmdBuffer;
	}

	void SwapChain::EndFrame(Arena& tempArena, const App& app, const Array<VkSemaphore>& waitSemaphores)
	{
		const auto scope = tempArena.CreateScope();

		const auto& frame = frames[frameIndex];
		const auto& image = images[imageIndex];

		vkCmdEndRenderPass(image.cmdBuffer);
		auto result = vkEndCommandBuffer(image.cmdBuffer);
		assert(!result);

		const auto allWaitSemaphores = CreateArray<VkSemaphore>(tempArena, waitSemaphores.length + 1);
		memcpy(allWaitSemaphores.ptr, waitSemaphores.ptr, sizeof(VkSemaphore) * waitSemaphores.length);
		allWaitSemaphores[waitSemaphores.length] = frame.imageAvailableSemaphore;

		const auto waitStages = CreateArray<VkPipelineStageFlags>(tempArena, allWaitSemaphores.length);
		for (auto& waitStage : waitStages)
			waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &image.cmdBuffer;
		submitInfo.waitSemaphoreCount = static_cast<uint32_t>(allWaitSemaphores.length);
		submitInfo.pWaitSemaphores = allWaitSemaphores.ptr;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &frame.renderFinishedSemaphore;
		submitInfo.pWaitDstStageMask = waitStages.ptr;

		vkResetFences(app.device, 1, &frame.inFlightFence);
		result = vkQueueSubmit(app.queues[App::renderQueue], 1, &submitInfo, frame.inFlightFence);
		assert(!result);

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &frame.renderFinishedSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(app.queues[App::presentQueue], &presentInfo);
		frameIndex = (frameIndex + 1) % frames.length;

		if (result)
			Recreate(tempArena, app, GetResolution());

		tempArena.DestroyScope(scope);
	}

	uint32_t SwapChain::GetLength() const
	{
		return images.length;
	}

	glm::ivec2 SwapChain::GetResolution() const
	{
		return { extent.width, extent.height };
	}

	VkCommandBuffer SwapChain::GetCmdBuffer() const
	{
		return images[imageIndex].cmdBuffer;
	}

	uint32_t SwapChain::GetIndex() const
	{
		return imageIndex;
	}

	VkRenderPass SwapChain::GetRenderPass() const
	{
		return renderPass;
	}

	VkFormat SwapChain::GetFormat() const
	{
		return surfaceFormat.format;
	}

	SwapChain SwapChain::Create(Arena& arena, Arena& tempArena, const App& app, const glm::ivec2 resolution)
	{
		SwapChain swapChain{};

		const auto scope = tempArena.CreateScope();

		const auto support = init::QuerySwapChainSupport(tempArena, app.physicalDevice, app.surface);
		const uint32_t imageCount = support.GetRecommendedImageCount();
		swapChain.surfaceFormat = ChooseSurfaceFormat(support.formats);
		swapChain.presentMode = ChoosePresentMode(support.presentModes);

		swapChain.images = CreateArray<Image>(arena, imageCount);
		swapChain.frames = CreateArray<Frame>(arena, SWAPCHAIN_MAX_FRAMES_IN_FLIGHT);

		for (auto& image : swapChain.images)
			image = {};

		swapChain.Recreate(tempArena, app, resolution);
		tempArena.DestroyScope(scope);

		return swapChain;
	}

	void SwapChain::Destroy(Arena& arena, const App& app, const SwapChain& swapChain)
	{
		Cleanup(app, swapChain);

		DestroyArray(arena, swapChain.frames);
		DestroyArray(arena, swapChain.images);
	}
}
