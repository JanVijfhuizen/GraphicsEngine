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
		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				auto normalTask = PixelPerfectRenderTask::ToNormalTask(task, _createInfo.resolution, _createInfo.simulatedResolution);

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
