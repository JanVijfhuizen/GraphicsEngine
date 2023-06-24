#pragma once
#include "Engine.h"
#include "Engine/TaskSystem.h"
#include "GE/GraphicsEngine.h"
#include "Tasks/InstancedRenderTask.h"

namespace game
{
	struct InstancedRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;

		const char* fragPath = "Shaders/frag.spv";
		const char* vertPath = "Shaders/vert.spv";
		bool drawsDirectlyToSwapChain = true;
	};

	struct InstancedRenderInterpreterEnableInfo final
	{
		jv::ge::Resource scene;
		uint32_t capacity;
	};

	class InstancedRenderInterpreter final : public TaskInterpreter<InstancedRenderTask, InstancedRenderInterpreterCreateInfo>
	{
	public:
		struct PushConstant final
		{
			struct Camera
			{
				glm::vec2 position{};
				float zoom = 0;
				float rotation = 0;
			} camera{};
			float rotation = 0;
		} pushConstant{};

		jv::ge::Resource image = nullptr;
		jv::ge::Resource mesh = nullptr;
		
		void Enable(const InstancedRenderInterpreterEnableInfo& info);
		void Disable();

	private:
		bool _enabled = false;
		InstancedRenderInterpreterCreateInfo _createInfo;

		jv::ge::Resource _shader;
		jv::ge::Resource _layout;
		jv::ge::Resource _renderPass;
		jv::ge::Resource _pipeline;

		jv::ge::Resource _buffer;
		jv::ge::Resource _fallbackImage;
		jv::ge::Resource _fallbackMesh;
		jv::ge::Resource _sampler;
		jv::ge::Resource _pool;

		void OnStart(const InstancedRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<InstancedRenderTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
