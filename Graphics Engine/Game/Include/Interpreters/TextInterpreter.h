﻿#pragma once
#include "Engine/Engine.h"
#include "GE/AtlasGenerator.h"
#include "Tasks/PixelPerfectRenderTask.h"
#include "Tasks/TextTask.h"

namespace game
{
	template <typename T>
	class InstancedRenderInterpreter;

	struct TextInterpreterCreateInfo final
	{
		TaskSystem<PixelPerfectRenderTask>* renderTasks;
		glm::ivec2 atlasResolution;
		jv::ge::AtlasTexture alphabetAtlasTexture;
		jv::ge::AtlasTexture numberAtlasTexture;
		jv::ge::AtlasTexture symbolAtlasTexture;
		uint32_t symbolSize = 9;
		int32_t spacing = -2;
		uint32_t bounceHeight = 3;
		float fadeInSpeed = 20;
		float bounceDuration = 4;
	};

	class TextInterpreter final : public TaskInterpreter<TextTask, TextInterpreterCreateInfo>
	{
	public:
		[[nodiscard]] static const char* Concat(const char* a, const char* b, jv::Arena& arena);
		[[nodiscard]] static const char* IntToConstCharPtr(uint32_t i, jv::Arena& arena);

	private:
		TextInterpreterCreateInfo _createInfo;
		
		void OnStart(const TextInterpreterCreateInfo& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<TextTask>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};
}
