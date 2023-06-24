#pragma once
#include "Engine.h"
#include "Engine/TaskSystem.h"
#include "GE/GraphicsEngine.h"
#include "Tasks/RenderTask.h"

namespace game
{
	struct BasicRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;
		uint32_t frameCount;
		uint32_t capacity;
		jv::ge::Resource scene;

		const char* fragPath = "Shaders/frag.spv";
		const char* vertPath = "Shaders/vert.spv";
		bool drawsDirectlyToSwapChain = true;
	};

	class BasicRenderInterpreter final : public TaskInterpreter<RenderTask, BasicRenderInterpreterCreateInfo>
	{
		jv::ge::Resource _shader;
		jv::ge::Resource _layout;
		jv::ge::Resource _renderPass;
		jv::ge::Resource _pipeline;
		jv::ge::Resource _buffer;

		void OnStart(const BasicRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<RenderTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
