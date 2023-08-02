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
		uint32_t upscaleMul = PixelPerfectRenderTask::GetUpscaleMultiplier(_createInfo.resolution, _createInfo.simulatedResolution);

		const auto pixelSize = glm::vec2(1) / glm::vec2(_createInfo.resolution.y) * glm::vec2(upscaleMul);
		const auto lBot = pixelSize * glm::vec2(_createInfo.simulatedResolution) * glm::vec2(-1, 1);

		PixelPerfectRenderTask bgRenderTask{};
		bgRenderTask.color = glm::vec4(0, 0, 0, 1);
		bgRenderTask.scale = _createInfo.simulatedResolution;
		bgRenderTask.subTexture = _createInfo.background;
		const auto normalBgTask = PixelPerfectRenderTask::ToNormalTask(bgRenderTask, _createInfo.resolution, _createInfo.simulatedResolution);
		_createInfo.renderTasks->Push(normalBgTask);
		
		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				auto normalTask = PixelPerfectRenderTask::ToNormalTask(task, _createInfo.resolution, _createInfo.simulatedResolution);
				if (!task.priority)
					_createInfo.renderTasks->Push(normalTask);
				else
					_createInfo.priorityRenderTasks->Push(normalTask); 
			}
	}

	void PixelPerfectRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
