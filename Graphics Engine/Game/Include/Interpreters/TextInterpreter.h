#pragma once
#include "Engine/Engine.h"
#include "GE/SubTexture.h"
#include "Tasks/RenderTask.h"
#include "Tasks/TextTask.h"

namespace game
{
	template <typename T>
	class InstancedRenderInterpreter;

	struct TextInterpreterCreateInfo final
	{
		TaskSystem<RenderTask>* instancedRenderTasks;
		glm::ivec2 atlasResolution;
		jv::ge::SubTexture alphabetSubTexture;
		jv::ge::SubTexture numberSubTexture;
		jv::ge::SubTexture symbolSubTexture;
		uint32_t symbolSize = 9;
	};

	class TextInterpreter final : public TaskInterpreter<TextTask, TextInterpreterCreateInfo>
	{
	public:
		[[nodiscard]] static const char* IntToConstCharPtr(uint32_t i, jv::Arena& arena);

	private:
		TextInterpreterCreateInfo _createInfo;
		
		void OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
