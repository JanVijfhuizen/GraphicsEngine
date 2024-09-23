#include "pch.h"
#include "Vk/VkImage.h"

#include "Vk/VkApp.h"
#include "Vk/VkFreeArena.h"

namespace jv::vk
{
	void GetLayoutMasks(const VkImageLayout layout, VkAccessFlags& outAccessFlags,
		VkPipelineStageFlags& outPipelineStageFlags)
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			outAccessFlags = 0;
			outPipelineStageFlags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			outAccessFlags = VK_ACCESS_TRANSFER_WRITE_BIT;
			outPipelineStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			outAccessFlags = VK_ACCESS_SHADER_READ_BIT;
			outPipelineStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			outAccessFlags = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			outPipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			outAccessFlags = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			outPipelineStageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			break;
		default:
			throw std::exception("Layout transition not supported!");
		}
	}

	void Image::TransitionLayout(const VkCommandBuffer cmd, const VkImageLayout newLayout,
		const VkImageAspectFlags aspectFlags)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = layout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectFlags;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage = 0;
		VkPipelineStageFlags dstStage = 0;

		GetLayoutMasks(layout, barrier.srcAccessMask, srcStage);
		GetLayoutMasks(newLayout, barrier.dstAccessMask, dstStage);

		vkCmdPipelineBarrier(cmd,
			srcStage, dstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		layout = newLayout;
	}

	void Image::FillImage(Arena& arena, const FreeArena& freeArena, const App& app, unsigned char* pixels, 
		const VkCommandBuffer cmd, glm::ivec2* overrideResolution)
	{
		assert(usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

		auto oResolution = overrideResolution ? *overrideResolution : resolution;
		const uint32_t imageSize = oResolution.x * oResolution.y * 4;

		VkBuffer stagingBuffer;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = imageSize;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		auto result = vkCreateBuffer(app.device, &bufferInfo, nullptr, &stagingBuffer);
		assert(!result);

		VkMemoryRequirements stagingMemRequirements;
		vkGetBufferMemoryRequirements(app.device, stagingBuffer, &stagingMemRequirements);

		Memory stagingMem{};
		const auto stagingMemHandle = freeArena.Alloc(arena, app, stagingMemRequirements,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, stagingMem);
		result = vkBindBufferMemory(app.device, stagingBuffer, stagingMem.memory, stagingMem.offset);
		assert(!result);

		// Copy pixels to staging buffer.
		void* data;
		vkMapMemory(app.device, stagingMem.memory, stagingMem.offset, imageSize, 0, &data);
		memcpy(data, pixels, imageSize);
		vkUnmapMemory(app.device, stagingMem.memory);
		
		vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

		VkFence fence;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		result = vkCreateFence(app.device, &fenceInfo, nullptr, &fence);
		assert(!result);
		result = vkResetFences(app.device, 1, &fence);
		assert(!result);

		VkCommandBufferBeginInfo cmdBeginInfo{};
		cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cmd, &cmdBeginInfo);

		auto currentLayout = layout;
		TransitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, aspectFlags);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent =
		{
			static_cast<uint32_t>(oResolution.x),
			static_cast<uint32_t>(oResolution.y),
			1
		};

		vkCmdCopyBufferToImage(
			cmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		TransitionLayout(cmd, currentLayout, aspectFlags);

		// End recording.
		result = vkEndCommandBuffer(cmd);
		assert(!result);

		VkSubmitInfo cmdSubmitInfo{};
		cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		cmdSubmitInfo.commandBufferCount = 1;
		cmdSubmitInfo.pCommandBuffers = &cmd;
		cmdSubmitInfo.waitSemaphoreCount = 0;
		cmdSubmitInfo.pWaitSemaphores = nullptr;
		cmdSubmitInfo.signalSemaphoreCount = 0;
		cmdSubmitInfo.pSignalSemaphores = nullptr;
		cmdSubmitInfo.pWaitDstStageMask = nullptr;
		result = vkQueueSubmit(app.queues[App::Queue::renderQueue], 1, &cmdSubmitInfo, fence);
		assert(!result);

		result = vkWaitForFences(app.device, 1, &fence, VK_TRUE, UINT64_MAX);
		assert(!result);

		vkDestroyFence(app.device, fence, nullptr);
		vkDestroyBuffer(app.device, stagingBuffer, nullptr);
		freeArena.Free(stagingMemHandle);
	}

	Image Image::Create(Arena& arena, const FreeArena& freeArena, const App& app, 
		const ImageCreateInfo& info)
	{
		Image image{};
		image.resolution = info.resolution;
		image.format = info.format;
		image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.aspectFlags = info.aspectFlags;
		image.usageFlags = info.usageFlags;

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = info.resolution.x;
		imageCreateInfo.extent.height = info.resolution.y;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.format = info.format;
		imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = info.usageFlags;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		auto result = vkCreateImage(app.device, &imageCreateInfo, nullptr, &image.image);
		assert(!result);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(app.device, image.image, &memRequirements);

		image.memoryHandle = freeArena.Alloc(arena, app, memRequirements,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1, image.memory);
		result = vkBindImageMemory(app.device, image.image, image.memory.memory, image.memory.offset);
		assert(!result);

		if (info.layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			vkResetCommandBuffer(info.cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

			VkFence fence;
			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			result = vkCreateFence(app.device, &fenceInfo, nullptr, &fence);
			assert(!result);
			result = vkResetFences(app.device, 1, &fence);
			assert(!result);

			VkCommandBufferBeginInfo cmdBeginInfo{};
			cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(info.cmd, &cmdBeginInfo);

			image.TransitionLayout(info.cmd, info.layout, image.aspectFlags);

			// End recording.
			result = vkEndCommandBuffer(info.cmd);
			assert(!result);

			VkSubmitInfo cmdSubmitInfo{};
			cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			cmdSubmitInfo.commandBufferCount = 1;
			cmdSubmitInfo.pCommandBuffers = &info.cmd;
			cmdSubmitInfo.waitSemaphoreCount = 0;
			cmdSubmitInfo.pWaitSemaphores = nullptr;
			cmdSubmitInfo.signalSemaphoreCount = 0;
			cmdSubmitInfo.pSignalSemaphores = nullptr;
			cmdSubmitInfo.pWaitDstStageMask = nullptr;
			result = vkQueueSubmit(app.queues[App::Queue::renderQueue], 1, &cmdSubmitInfo, fence);
			assert(!result);

			result = vkWaitForFences(app.device, 1, &fence, VK_TRUE, UINT64_MAX);
			assert(!result);

			vkDestroyFence(app.device, fence, nullptr);
		}

		return image;
	}

	void Image::Destroy(const FreeArena& freeArena, const App& app, const Image& image)
	{
		vkDestroyImage(app.device, image.image, nullptr);
		freeArena.Free(image.memoryHandle);
	}
}
