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

	struct RenderGraphResource
	{
		uint32_t pool;
		uint32_t instance;
	};

	struct RenderGraph final
	{
		struct Pass final
		{
			Array<RenderGraphResource> inResources{};
			Array<RenderGraphResource> outResources{};
			uint32_t nodeIndex;
		};

		struct Batch final
		{
			Array<Pass> passes{};
		};

		struct Pool final
		{
			ResourceMaskDescription resourceMaskDescription;
			uint32_t capacity;
		};

		uint64_t scope;
		Array<Pool> pools{};
		Array<Batch> batches{};

		[[nodiscard]] static RenderGraph Create(Arena& arena, Arena& tempArena, const RenderGraphCreateInfo& info);
		void Debug() const;
		static void Destroy(Arena& arena, const RenderGraph& renderGraph);
	};
}
