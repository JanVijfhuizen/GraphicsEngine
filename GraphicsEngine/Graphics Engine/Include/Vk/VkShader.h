#pragma once

namespace jv::vk
{
	struct App;

	[[nodiscard]] VkShaderModule CreateShaderModule(const App& app, const char* code, uint64_t length);
}
