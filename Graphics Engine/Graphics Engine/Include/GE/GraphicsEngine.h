#pragma once

namespace jv::ge
{
	struct CreateInfo final
	{
		const char* name = "Graphics Engine";
		glm::ivec2 resolution{ 800, 600 };
		bool fullscreen = false;
	};

	void Initialize(const CreateInfo& info);
	void Resize(glm::ivec2 resolution, bool fullScreen);
	[[nodiscard]] bool RenderFrame();
	void Shutdown();
}
