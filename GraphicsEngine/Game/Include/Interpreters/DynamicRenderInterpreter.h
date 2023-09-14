#pragma once
#include "Engine/Engine.h"
#include "Tasks/DynamicRenderTask.h"
#include "Tasks/RenderTask.h"

namespace game
{
	struct DynamicRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;
		jv::Arena* frameArena;

		const char* fragPath = "Shaders/frag-dyn.spv";
		const char* vertPath = "Shaders/vert-dyn.spv";
		bool drawsDirectlyToSwapChain = true;
	};

	struct DynamicRenderInterpreterEnableInfo final
	{
		jv::Arena* arena;
		jv::Arena* frameArena;
		jv::ge::Resource scene;
		uint32_t capacity;
	};

	class DynamicRenderInterpreter final : public TaskInterpreter<DynamicRenderTask, DynamicRenderInterpreterCreateInfo>
	{
	public:
		struct Camera
		{
			glm::vec2 position{};
			float zoom = 0;
			float rotation = 0;
		} camera{};

		void Enable(const DynamicRenderInterpreterEnableInfo& info);
		jv::ge::Resource GetFallbackMesh() const;

	private:
		struct PushConstant final
		{
			RenderTask renderTask;
			Camera camera{};
			glm::vec2 resolution;
		};
		
		DynamicRenderInterpreterCreateInfo _createInfo;
		uint32_t _capacity;

		jv::ge::Resource _shader;
		jv::ge::Resource _layout;
		jv::ge::Resource _renderPass;
		jv::ge::Resource _pipeline;

		jv::ge::Resource _buffer;
		jv::ge::Resource _fallbackImage;
		jv::ge::Resource _fallbackNormalImage;
		jv::ge::Resource _fallbackMesh;
		jv::Array<jv::ge::Resource> _samplers;
		jv::ge::Resource _pool;
		uint32_t _frameCapacity;

		void OnStart(const DynamicRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<DynamicRenderTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
