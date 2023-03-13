#include "pch.h"
#include "Vk/VkLayout.h"

#include "JLib/ArrayUtils.h"
#include "Vk/VkApp.h"

namespace jv::vk
{
	VkDescriptorSetLayout CreateLayout(Arena& tempArena, const App& app, const Array<Binding>& bindings)
	{
		const auto scope = tempArena.CreateScope();
		const auto sets = CreateArray<VkDescriptorSetLayoutBinding>(tempArena, bindings.length);

		for (uint32_t i = 0; i < bindings.length; ++i)
		{
			const auto& binding = bindings[i];
			auto& set = sets[i];

			set.stageFlags = binding.flag;
			set.binding = i;
			set.descriptorCount = binding.count;
			set.descriptorType = binding.type;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.pNext = nullptr;
		layoutInfo.flags = 0;
		layoutInfo.bindingCount = sets.length;
		layoutInfo.pBindings = sets.ptr;

		VkDescriptorSetLayout layout;
		const auto result = vkCreateDescriptorSetLayout(app.device, &layoutInfo, nullptr, &layout);
		assert(!result);

		tempArena.DestroyScope(scope);
		return layout;
	}
}
