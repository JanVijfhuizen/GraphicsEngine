#include "pch.h"
#include "Vk/VkShader.h"

#include "Vk/VkApp.h"

namespace jv::vk
{
	VkShaderModule CreateShaderModule(const App& app, const char* code, const uint64_t length)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

		createInfo.codeSize = length;
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

		VkShaderModule mod;
		const auto result = vkCreateShaderModule(app.device, &createInfo, nullptr, &mod);
		assert(!result);
		return mod;
	}
}
