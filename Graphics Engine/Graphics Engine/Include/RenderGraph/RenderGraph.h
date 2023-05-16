#pragma once

namespace ge
{
	typedef uint64_t ResourceMaskDescription;

	struct RenderGraphNodeInfo final
	{
		uint32_t* inResources;
		uint32_t inResourceCount;
		uint32_t* outResources;
		uint32_t outResourceCount;
	};

	struct RenderGraphCreateInfo final
	{
		ResourceMaskDescription* resources;
		uint32_t resourceCount;
		RenderGraphNodeInfo* nodes;
		uint32_t nodeCount;
	};

	struct RenderGraph final
	{
		[[nodiscard]] static RenderGraph Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info);
		static void Destroy(jv::Arena& arena, const RenderGraph& renderGraph);
	};
}
