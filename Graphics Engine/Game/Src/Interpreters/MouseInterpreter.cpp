#include "pch_game.h"
#include "Interpreters/MouseInterpreter.h"

namespace game
{
	void MouseInterpreter::OnStart(const MouseInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_renderTasks = createInfo.renderTasks;
	}

	void MouseInterpreter::OnUpdate(const EngineMemory& memory,
		const jv::LinkedList<jv::Vector<MouseTask>>& tasks)
	{
		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				RenderTask mouseRenderTask{};
				mouseRenderTask.position = job.position;
				mouseRenderTask.position *= 2;
				mouseRenderTask.position -= glm::vec2(1);
				mouseRenderTask.scale *= .2f;
				mouseRenderTask.subTexture = mouseIdleSubTexture;
				_renderTasks->Push(mouseRenderTask);
			}
	}

	void MouseInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
