﻿#pragma once
#include "VkMemory.h"
#include "JLib/Array.h"

namespace jv::vk
{
	struct FreeArena;
	struct App;

	struct ImageCreateInfo final
	{
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	};

	struct Image final
	{
		VkImage image;
		VkImageLayout layout;
		VkFormat format;
		VkImageAspectFlags aspectFlags;
		glm::ivec3 resolution;
		Memory memory;
		uint64_t memoryHandle;

		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);

		[[nodiscard]] static Image CreateImage(Arena& arena, const FreeArena& freeArena, const App& app, const ImageCreateInfo& info, glm::ivec3 resolution);
		[[nodiscard]] static Image CreateImage(Arena& arena, FreeArena& freeArena, const App& app, ImageCreateInfo info,
			const Array<unsigned char>& pixels, glm::ivec3 resolution);
		static void DestroyImage(const FreeArena& freeArena, const App& app, const Image& image);
	};
}
