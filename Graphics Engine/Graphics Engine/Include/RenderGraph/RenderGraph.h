#pragma once
#include "JLib/Array.h"

namespace jv::rg
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
		struct Pass final
		{
			jv::Array<uint32_t> inResourcePools{};
			jv::Array<uint32_t> outResourcePools{};
			uint32_t index;
		};

		struct Batch final
		{
			jv::Array<Pass> passes{};
		};

		struct Pool final
		{
			ResourceMaskDescription resourceMaskDescription;
			uint32_t capacity;
		};

		uint64_t scope;
		jv::Array<Pool> pools{};
		jv::Array<Batch> batches{};

		[[nodiscard]] static RenderGraph Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info);
		static void Destroy(jv::Arena& arena, const RenderGraph& renderGraph);
	};
}
