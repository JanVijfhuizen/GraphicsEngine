#pragma once
#include "Engine/TaskSystem.h"
#include "GE/GraphicsEngine.h"

namespace game
{
	struct BasicRenderSystemCreateInfo final
	{
		glm::ivec2 resolution;
		uint32_t frameCount;
		uint32_t storageBufferSize;
		jv::ge::Resource scene;

		const char* fragPath = "Shaders/frag.spv";
		const char* vertPath = "Shaders/vert.spv";
		bool drawsDirectlyToSwapChain = true;
	};

	struct BasicRenderSystem final
	{
		jv::ge::Resource shader;
		jv::ge::Resource layout;
		jv::ge::Resource renderPass;
		jv::ge::Resource pipeline;
		jv::ge::Resource buffer;

		[[nodiscard]] static BasicRenderSystem Create(jv::Arena& tempArena, const BasicRenderSystemCreateInfo& info);
		static void Destroy(const BasicRenderSystem& renderSystem);
	};
}
