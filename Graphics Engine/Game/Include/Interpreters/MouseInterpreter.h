#pragma once
#include "Engine/Engine.h"
#include "Tasks/MouseTask.h"
#include "Tasks/RenderTask.h"

namespace game
{
	struct MouseInterpreterCreateInfo final
	{
		TaskSystem<RenderTask>* renderTasks = nullptr;
	};

	class MouseInterpreter final : public TaskInterpreter<MouseTask, MouseInterpreterCreateInfo>
	{
	public:
		jv::ge::SubTexture mouseIdleSubTexture;
		jv::ge::SubTexture mouseLClickSubTexture;

	private:
		TaskSystem<RenderTask>* _renderTasks;

		void OnStart(const MouseInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<MouseTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
