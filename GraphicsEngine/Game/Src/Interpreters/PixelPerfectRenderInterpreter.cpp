#include "pch_game.h"
#include "Interpreters/PixelPerfectRenderInterpreter.h"

#include "JLib/Math.h"

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
		glm::ivec2 screenShakeOffset{};
		if(_createInfo.screenShakeInfo->remaining > 0)
		{
			auto info = _createInfo.screenShakeInfo;
			const float mul = jv::Min(info->fallOfThreshold, info->remaining) / info->fallOfThreshold;
			const int32_t intensity = info->intensity;
			screenShakeOffset.x = (rand() % intensity * 2 - intensity) * mul;
			screenShakeOffset.y = (rand() % intensity * 2 - intensity) * mul;
		}

		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				PixelPerfectRenderTask cpyTask = task;
				cpyTask.position += screenShakeOffset;
				
				auto normalTask = PixelPerfectRenderTask::ToNormalTask(cpyTask, _createInfo.resolution, _createInfo.simulatedResolution);

				if(task.image)
				{
					DynamicRenderTask dynTask{};
					dynTask.renderTask = normalTask;
					dynTask.image = task.image;
					dynTask.normalImage = task.normalImage;
					if (!task.priority)
						_createInfo.dynRenderTasks->Push(dynTask);
					else
						_createInfo.dynPriorityRenderTasks->Push(dynTask);
				}
				else
				{
					if (task.front)
					{
						_createInfo.frontRenderTasks->Push(normalTask);
						continue;
					}

					if (!task.priority)
						_createInfo.renderTasks->Push(normalTask);
					else
						_createInfo.priorityRenderTasks->Push(normalTask);
				}
			}
	}

	void PixelPerfectRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
