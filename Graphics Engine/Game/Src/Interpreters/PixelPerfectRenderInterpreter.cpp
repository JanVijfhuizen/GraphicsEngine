#include "pch_game.h"
#include "Interpreters/PixelPerfectRenderInterpreter.h"

namespace game
{
	void PixelPerfectRenderInterpreter::OnStart(const PixelPerfectRenderInterpreterCreateInfo& createInfo,
		const EngineMemory& memory)
	{
		_createInfo = createInfo;
	}

	void PixelPerfectRenderInterpreter::OnUpdate(const EngineMemory& memory,
		const jv::LinkedList<jv::Vector<PixelPerfectRenderTask>>& tasks)
	{
		glm::ivec2 r = _createInfo.simulatedResolution;
		uint32_t upscaleMul = 1;
		while (r.x * 2 < _createInfo.resolution.x || r.y * 2 < _createInfo.resolution.y)
		{
			r *= 2;
			upscaleMul *= 2;
		}

		const auto pixelSize = glm::vec2(1) / glm::vec2(_createInfo.resolution.y) * glm::vec2(upscaleMul);
		const auto lBot = pixelSize * glm::vec2(_createInfo.simulatedResolution) * glm::vec2(-1, 1);

		RenderTask bgRenderTask{};
		bgRenderTask.color = glm::vec4(0, 0, 0, 1);
		bgRenderTask.scale = pixelSize * glm::vec2(_createInfo.simulatedResolution);
		bgRenderTask.subTexture = _createInfo.background;
		_createInfo.renderTasks->Push(bgRenderTask);
		
		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				RenderTask renderTask{};
				renderTask.scale = pixelSize * glm::vec2(task.scale.x, task.scale.y);
				renderTask.position = lBot + pixelSize * glm::vec2(2) * glm::vec2(task.position.x, -task.position.y) + renderTask.scale * glm::vec2(1, -1);
				renderTask.color = task.color;
				renderTask.subTexture = task.subTexture;

				if (!task.priority)
					_createInfo.renderTasks->Push(renderTask);
				else
					_createInfo.priorityRenderTasks->Push(renderTask);
			}
	}

	void PixelPerfectRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
