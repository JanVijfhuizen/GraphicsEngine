#include "pch.h"
#include "Vk/VkShader.h"

#include "Vk/VkApp.h"

namespace jv::vk
{
	VkShaderModule CreateShaderModule(const App& app, const char* code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

		createInfo.codeSize = strlen(code);
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

		VkShaderModule mod;
		const auto result = vkCreateShaderModule(app.device, &createInfo, nullptr, &mod);
		assert(!result);
		return mod;
	}
}
