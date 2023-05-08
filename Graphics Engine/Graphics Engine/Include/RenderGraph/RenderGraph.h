#pragma once

namespace ge
{
	struct RenderGraphResourceInfo final
	{
		enum class Type
		{
			color,
			depth
		} type = Type::color;
		glm::ivec2 resolution;
	};

	struct RenderGraphNodeInfo final
	{
		uint32_t* inResources;
		uint32_t inResourceCount;
		uint32_t* outResources;
		uint32_t outResourceCount;
	};

	struct RenderGraphCreateInfo final
	{
		RenderGraphResourceInfo* resources;
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
