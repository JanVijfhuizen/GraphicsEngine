#pragma once
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

	// Image that can be used for things like textures or post effects.
	struct Image final
	{
		VkImage image;
		VkImageLayout layout;
		VkFormat format;
		VkImageAspectFlags aspectFlags;
		VkImageUsageFlags usageFlags;
		glm::ivec3 resolution;
		Memory memory;
		uint64_t memoryHandle;

		// Transition the layout for it to be used in different ways, like for a depth attachment, or a sampled image.
		void TransitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout, VkImageAspectFlags aspectFlags);
		void FillImage(Arena& arena, const FreeArena& freeArena, const App& app, unsigned char* pixels);

		[[nodiscard]] static Image Create(Arena& arena, const FreeArena& freeArena, const App& app, const ImageCreateInfo& info, glm::ivec3 resolution);
		static void Destroy(const FreeArena& freeArena, const App& app, const Image& image);
	};
}
