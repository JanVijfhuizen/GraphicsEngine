#pragma once
#include "JLib/Array.h"
#include "Vk/VkApp.h"

namespace jv
{
	struct Arena;
}

namespace jv::vk::init
{
	struct QueueFamilies final
	{
		uint32_t graphics;
		uint32_t present;
		uint32_t transfer;

		[[nodiscard]] operator bool() const;
	};

	struct PhysicalDeviceInfo final
	{
		VkPhysicalDevice device;
		VkPhysicalDeviceProperties properties;
		VkPhysicalDeviceFeatures features;
	};

	struct SwapChainSupportDetails final
	{
		VkSurfaceCapabilitiesKHR capabilities{};
		Array<VkSurfaceFormatKHR> formats{};
		Array<VkPresentModeKHR> presentModes{};

		[[nodiscard]] operator bool() const;
		[[nodiscard]] uint32_t GetRecommendedImageCount() const;
	};

	[[nodiscard]] bool IsPhysicalDeviceValid(const PhysicalDeviceInfo& info);
	[[nodiscard]] uint32_t GetPhysicalDeviceRating(const PhysicalDeviceInfo& info);
	[[nodiscard]] VkPhysicalDeviceFeatures GetPhysicalDeviceFeatures();

	// Vulkan application create info.
	struct Info final
	{
		Arena* tempArena;
		VkSurfaceKHR(*createSurface)(VkInstance instance, void* userPtr);
		void* userPtr;

		Array<const char*> validationLayers{};
		Array<const char*> instanceExtensions{};
		Array<const char*> deviceExtensions{};

		bool(*isPhysicalDeviceValid)(const PhysicalDeviceInfo& info) = IsPhysicalDeviceValid;
		uint32_t(*getPhysicalDeviceRating)(const PhysicalDeviceInfo& info) = GetPhysicalDeviceRating;
		VkPhysicalDeviceFeatures(*getPhysicalDeviceFeatures)() = GetPhysicalDeviceFeatures;
	};

	// Initialize a vulkan app that work for most common use cases like pc games.
	[[nodiscard]] App CreateApp(const Info& info);
	void DestroyApp(const App& app);

	[[nodiscard]] QueueFamilies GetQueueFamilies(Arena& arena, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
	[[nodiscard]] SwapChainSupportDetails QuerySwapChainSupport(Arena& arena, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);
}
