#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/Math.h"

namespace ge
{
	struct NodeMetaData final
	{
		uint32_t bandWidth = 0;
	};

	struct ResourceMetaData final
	{
		uint32_t src;
		jv::LinkedList<uint32_t> dsts{};
	};

	bool RenderGraphResourceInfo::operator==(const RenderGraphResourceInfo& other) const
	{
		return other.resolution == resolution && other.type == type;
	}

	bool RenderGraphResourceInfo::operator!=(const RenderGraphResourceInfo& other) const
	{
		return !(other == *this);
	}

	void DefineResourceConnections(jv::Arena& arena, ResourceMetaData* metaDatas, const RenderGraphCreateInfo& info)
	{
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			metaDatas[i] = {};

		for (uint32_t i = 0; i < info.nodeCount; ++i)
		{
			const auto& node = info.nodes[i];

			for (uint32_t j = 0; j < node.outResourceCount; ++j)
			{
				const uint32_t resourceIndex = node.outResources[j];
				metaDatas[resourceIndex].src = i;
			}

			for (uint32_t j = 0; j < node.inResourceCount; ++j)
			{
				const uint32_t resourceIndex = node.inResources[j];
				Add(arena, metaDatas[resourceIndex].dsts) = i;
			}
		}
	}

	uint32_t FindRoot(const RenderGraphCreateInfo& info)
	{
		uint32_t root = -1;
		for (uint32_t i = 0; i < info.nodeCount; ++i)
			root = info.nodes[i].outResourceCount == 0 ? i : root;
		return root;
	}

	uint32_t DefineBandWidths(NodeMetaData* nodeMetaDatas, ResourceMetaData* resourceMetaDatas, const RenderGraphCreateInfo& info, const uint32_t current)
	{
		const auto& node = info.nodes[current];

		uint32_t bandWidth = node.inResourceCount + node.outResourceCount;
		for (uint32_t i = 0; i < node.inResourceCount; ++i)
		{
			const auto& resourceMetaData = resourceMetaDatas[node.inResources[i]];
			uint32_t childBandWidth = DefineBandWidths(nodeMetaDatas, resourceMetaDatas, info, resourceMetaData.src);
			bandWidth = jv::Max(bandWidth, childBandWidth);
		}

		auto& currentMetaData = nodeMetaDatas[current];
		currentMetaData.bandWidth = jv::Max(currentMetaData.bandWidth, bandWidth);
		return bandWidth;
	}

	RenderGraph RenderGraph::Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();
		
		const auto nodeMetaDatas = jv::CreateArray<NodeMetaData>(tempArena, info.nodeCount);
		const auto resourceMetaDatas = jv::CreateArray<ResourceMetaData>(tempArena, info.resourceCount);

		DefineResourceConnections(tempArena, resourceMetaDatas.ptr, info);
		const uint32_t rootIndex = FindRoot(info);
		DefineBandWidths(nodeMetaDatas.ptr, resourceMetaDatas.ptr, info, rootIndex);

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
