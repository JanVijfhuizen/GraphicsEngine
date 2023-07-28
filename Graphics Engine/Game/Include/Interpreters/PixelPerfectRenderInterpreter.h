#pragma once
#include "Engine/Engine.h"
#include "Tasks/RenderTask.h"

namespace game
{
	struct PixelPerfectRenderTask final
	{
		glm::ivec2 position{};
		glm::ivec2 scale{32};
		glm::vec4 color{1};
		jv::ge::SubTexture subTexture{};
		bool priority = false;
	};

	struct PixelPerfectRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;
		glm::ivec2 simulatedResolution;
		TaskSystem<RenderTask>* renderTasks;
		TaskSystem<RenderTask>* priorityRenderTasks;
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
