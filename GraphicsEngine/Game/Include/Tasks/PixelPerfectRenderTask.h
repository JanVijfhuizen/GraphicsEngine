#pragma once
#include "RenderTask.h"
#include "GE/SubTexture.h"

namespace game
{
	struct PixelPerfectRenderTask final
	{
		glm::ivec2 position{};
		glm::ivec2 scale{ 32 };
		glm::vec4 color{ 1 };
		jv::ge::SubTexture subTexture{};
		bool xCenter = false;
		bool yCenter = false;
		bool priority = false;
		jv::ge::Resource image = nullptr;
		jv::ge::Resource normalImage = nullptr;

		[[nodiscard]] static uint32_t GetUpscaleMultiplier(glm::ivec2 resolution, glm::ivec2 simulatedResolution);
		[[nodiscard]] static glm::vec2 GetPixelSize(glm::vec2 resolution, uint32_t upscaleMul);
		[[nodiscard]] static glm::ivec2 ToPixelPosition(glm::ivec2 resolution, glm::ivec2 simulatedResolution, glm::vec2 position);
		[[nodiscard]] static RenderTask ToNormalTask(const PixelPerfectRenderTask& task, glm::ivec2 resolution, glm::ivec2 simulatedResolution);
	};
}
