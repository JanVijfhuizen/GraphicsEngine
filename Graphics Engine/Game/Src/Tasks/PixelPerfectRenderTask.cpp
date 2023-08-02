#include "pch_game.h"
#include "Tasks/PixelPerfectRenderTask.h"

namespace game
{
	glm::ivec2 PixelPerfectRenderTask::ToPixelPosition(
		const glm::ivec2 resolution, 
		const glm::ivec2 simulatedResolution,
		const glm::vec2 position)
	{
		const auto upscaleMul = GetUpscaleMultiplier(resolution, simulatedResolution);
		const auto pixelSize = GetPixelSize(resolution, upscaleMul);
		const auto lBot = pixelSize * glm::vec2(simulatedResolution) * glm::vec2(-1, 1);
		return lBot + (position + glm::vec2(.5f)) / pixelSize;
	}

	uint32_t PixelPerfectRenderTask::GetUpscaleMultiplier(const glm::ivec2 resolution, const glm::ivec2 simulatedResolution)
	{
		glm::ivec2 r = simulatedResolution;
		uint32_t upscaleMul = 1;
		while (r.x * 2 < resolution.x || r.y * 2 < resolution.y)
		{
			r *= 2;
			upscaleMul *= 2;
		}
		return upscaleMul;
	}

	glm::vec2  PixelPerfectRenderTask::GetPixelSize(const glm::vec2 resolution, const uint32_t upscaleMul)
	{
		return glm::vec2(1) / glm::vec2(resolution.y) * glm::vec2(static_cast<float>(upscaleMul));
	}

	RenderTask PixelPerfectRenderTask::ToNormalTask(const PixelPerfectRenderTask& task, glm::ivec2 resolution,
		glm::ivec2 simulatedResolution)
	{
		const auto upscaleMul = GetUpscaleMultiplier(resolution, simulatedResolution);
		const auto pixelSize = GetPixelSize(resolution, upscaleMul);
		const auto lBot = pixelSize * glm::vec2(simulatedResolution) * glm::vec2(-1, 1);

		RenderTask renderTask{};
		renderTask.scale = pixelSize * glm::vec2(task.scale.x, task.scale.y);
		renderTask.position = lBot + pixelSize * glm::vec2(2) * glm::vec2(task.position.x, -task.position.y) + renderTask.scale * glm::vec2(1, -1);
		renderTask.color = task.color;
		renderTask.subTexture = task.subTexture;
		return renderTask;
	}
}