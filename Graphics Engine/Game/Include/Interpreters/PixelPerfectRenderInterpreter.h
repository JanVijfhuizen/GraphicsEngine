#pragma once
#include "Engine/Engine.h"
#include "Tasks/PixelPerfectRenderTask.h"
#include "Tasks/RenderTask.h"

namespace game
{
	struct PixelPerfectRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;
		glm::ivec2 simulatedResolution;
		TaskSystem<RenderTask>* renderTasks;
		TaskSystem<RenderTask>* priorityRenderTasks;
		jv::ge::SubTexture background;
	};

	class PixelPerfectRenderInterpreter final : public TaskInterpreter<PixelPerfectRenderTask, PixelPerfectRenderInterpreterCreateInfo>
	{
		PixelPerfectRenderInterpreterCreateInfo _createInfo;

		void OnStart(const PixelPerfectRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory,
			const jv::LinkedList<jv::Vector<PixelPerfectRenderTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
