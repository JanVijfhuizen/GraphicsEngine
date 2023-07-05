#include "pch_game.h"
#include "Interpreters/MouseInterpreter.h"

#include "Utils/SubTextureUtils.h"

namespace game
{
	void MouseInterpreter::OnStart(const MouseInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
		_renderTasks = createInfo.renderTasks;
	}

	void MouseInterpreter::OnUpdate(const EngineMemory& memory,
		const jv::LinkedList<jv::Vector<MouseTask>>& tasks)
	{
		jv::ge::SubTexture subTextures[2];
		Divide(subTexture, subTextures, (sizeof subTextures) / sizeof(jv::ge::SubTexture));

		for (const auto& batch : tasks)
			for (const auto& job : batch)
			{
				switch (job.lButton)
				{
					case InputState::idle:
						break;
					case InputState::pressed:
						_lPressed = true;
						break;
					case InputState::released:
						_lPressed = false;
						break;
					default: 
						throw std::exception("Mouse button state not supported!");
				}

				RenderTask mouseRenderTask{};
				mouseRenderTask.position = job.position;
				mouseRenderTask.scale *= .08f;
				mouseRenderTask.position += mouseRenderTask.scale * glm::vec2(.5f, .5f);
				mouseRenderTask.subTexture = _lPressed ? subTextures[1] : subTextures[0];
				_renderTasks->Push(mouseRenderTask);
			}
	}

	void MouseInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
