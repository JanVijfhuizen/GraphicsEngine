#pragma once
#include "Engine/Engine.h"
#include "GE/SubTexture.h"
#include "Tasks/InstancedRenderTask.h"
#include "Tasks/TextTask.h"

namespace game
{
	class InstancedRenderInterpreter;

	struct TextInterpreterCreateInfo final
	{
		TaskSystem<InstancedRenderTask>* instancedRenderTasks;
		glm::ivec2 atlasResolution;
		jv::ge::SubTexture alphabetSubTexture;
		jv::ge::SubTexture numberSubTexture;
		jv::ge::SubTexture symbolSubTexture;

		uint32_t symbolSize = 8;
		uint32_t alphabetTextureIndex = 0;
		uint32_t numbersTextureIndex = 1;
		uint32_t symbolsTextureIndex = 2;
	};

	class TextInterpreter final : public TaskInterpreter<TextTask, TextInterpreterCreateInfo>
	{
		TextInterpreterCreateInfo _createInfo;
		
		void OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
