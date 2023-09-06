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
		const auto pixelSize = glm::vec2(1) / glm::vec2(resolution) * glm::vec2(static_cast<float>(upscaleMul));
		const auto lBot = pixelSize * glm::vec2(simulatedResolution) * glm::vec2(-1, 1);
		const auto rPos = glm::vec2(position.x, static_cast<float>(resolution.y) - position.y) / glm::vec2(upscaleMul);
		const auto diff = (resolution / glm::ivec2(static_cast<float>(upscaleMul)) - simulatedResolution) / 2;

		const auto res = glm::ivec2(rPos) - diff;
		return res;
	}

	uint32_t PixelPerfectRenderTask::GetUpscaleMultiplier(const glm::ivec2 resolution, const glm::ivec2 simulatedResolution)
	{
		glm::ivec2 r = simulatedResolution;
		uint32_t upscaleMul = 0;
		
		while(r.x <= resolution.x && r.y <= resolution.y)
		{
			++upscaleMul;
			r.x += simulatedResolution.x;
			r.y += simulatedResolution.y;
		}
		
		return upscaleMul;
	}

	glm::vec2  PixelPerfectRenderTask::GetPixelSize(const glm::vec2 resolution, const uint32_t upscaleMul)
	{
		return glm::vec2(1) / glm::vec2(resolution.y) * glm::vec2(static_cast<float>(upscaleMul));
	}

	RenderTask PixelPerfectRenderTask::ToNormalTask(const PixelPerfectRenderTask& task, glm::ivec2 resolution,
		glm::ivec2 simulatedResolution, const bool upscale)
	{
		const auto upscaleMul = upscale ? GetUpscaleMultiplier(resolution, simulatedResolution) : 1;
		const auto pixelSize = GetPixelSize(upscale ? simulatedResolution : resolution, upscaleMul);
		const auto lBot = pixelSize * glm::vec2(simulatedResolution) * glm::vec2(-1, 1);

		RenderTask renderTask{};
		renderTask.scale = pixelSize * glm::vec2(task.scale.x, task.scale.y);
		renderTask.position = lBot + pixelSize * glm::vec2(2) * glm::vec2(task.position.x, -task.position.y) + renderTask.scale * glm::vec2(1, -1);
		renderTask.color = task.color;
		renderTask.subTexture = task.subTexture;

		if (task.xCenter)
			renderTask.position.x -= renderTask.scale.x;
		if (task.yCenter)
			renderTask.position.y += renderTask.scale.y;
		return renderTask;
	}
}