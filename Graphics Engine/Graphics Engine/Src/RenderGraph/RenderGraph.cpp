#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedList.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/Math.h"
#include "JLib/VectorUtils.h"

namespace ge
{
	struct ResourceMetaData final
	{
		uint32_t src = 0;
		jv::LinkedList<uint32_t> dsts{};
		bool processed = false;
	};

	struct NodeMetaData final
	{
		uint32_t remaining;
	};

	jv::Array<uint32_t> GetLeafs(jv::Arena& arena, const RenderGraphCreateInfo& info)
	{
		uint32_t count = 0;
		for (uint32_t i = 0; i < info.nodeCount; ++i)
			count += info.nodes[i].inResourceCount == 0;
		const auto arr = jv::CreateArray<uint32_t>(arena, count);
		uint32_t index = 0;
		for (uint32_t i = 0; i < info.nodeCount; ++i)
		{
			const auto& node = info.nodes[i];
			if (node.inResourceCount > 0)
				continue;
			arr[index++] = i;
		}
		return arr;
	}

	uint32_t CalculateDepthComplexity(const RenderGraphCreateInfo& info,
		const jv::Array<ResourceMetaData>& resourceMetaDatas, const jv::Array<NodeMetaData>& nodeMetaDatas, 
		const uint32_t index, uint32_t depth)
	{
		if (nodeMetaDatas[index].remaining == 0)
			return depth;

		const auto& node = info.nodes[index];
		for (uint32_t i = 0; i < node.inResourceCount; ++i)
		{
			const auto& src = resourceMetaDatas[i].src;
			depth = jv::Max(depth, CalculateDepthComplexity(info, resourceMetaDatas, nodeMetaDatas, src, depth + node.inResourceCount));
		}
			
		return depth;
	}

	RenderGraph RenderGraph::Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();

		// Calculate graph starting points.
		const auto leafs = GetLeafs(arena, info);
		auto open = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);
		open.Add() = leafs[0];

		// Define meta data for resources and nodes.
		const auto resourceMetaDatas = jv::CreateArray<ResourceMetaData>(tempArena, info.resourceCount);
		const auto nodeMetaDatas = jv::CreateArray<NodeMetaData>(tempArena, info.nodeCount);

		// Fill important meta data.
		for (auto& metaData : resourceMetaDatas)
			metaData = {};
		for (uint32_t i = 0; i < info.nodeCount; ++i)
		{
			const auto& node = info.nodes[i];
			auto& metaData = nodeMetaDatas[i] = {};
			metaData.remaining = node.inResourceCount;

			for (uint32_t j = 0; j < node.inResourceCount; ++j)
				Add(tempArena, resourceMetaDatas[node.inResources[j]].dsts) = i;
			for (uint32_t j = 0; j < node.outResourceCount; ++j)
				resourceMetaDatas[node.outResources[j]].src = i;
		}

		// The ordered list of execution for when the render graph executes.
		auto ordered = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);

		while(open.count > 0)
		{
			const auto current = open[open.count - 1];
			const auto& currentMetaData = nodeMetaDatas[current];
			const auto& node = info.nodes[current];

			if(currentMetaData.remaining == 0)
			{
				ordered.Add() = current;
				open.RemoveAt(open.count - 1);

				for (uint32_t i = 0; i < node.outResourceCount; ++i)
				{
					const auto& resource = node.outResources[i];
					auto& resourceMetaData = resourceMetaDatas[resource];
					resourceMetaData.processed = true;

					for (const auto& dst : resourceMetaData.dsts)
					{
						auto& parentNodeMetaData = nodeMetaDatas[dst];
						--parentNodeMetaData.remaining;
					}
				}
				
				for (uint32_t i = 0; i < node.outResourceCount; ++i)
				{
					const auto& resource = node.outResources[i];
					auto& resourceMetaData = resourceMetaDatas[resource];

					if(open.count == 0)
					{
						const uint32_t count = resourceMetaData.dsts.GetCount();
						if(count > 0)
						{
							open.Add() = resourceMetaData.dsts[count - 1];
							break;
						}
					}
				}

				continue;
			}

			const auto complexities = jv::CreateArray<uint32_t>(tempArena, node.inResourceCount);

			for (uint32_t i = 0; i < node.inResourceCount; ++i)
			{
				const auto& resource = node.inResources[i];
				auto& resourceMetaData = resourceMetaDatas[resource];
				const auto& src = resourceMetaData.src;

				const auto complexity = CalculateDepthComplexity(info, resourceMetaDatas, nodeMetaDatas, src, 0);
				complexities[i] = complexity;

				if (resourceMetaData.processed)
					continue;
				open.Add() = src;
			}
		}

		// Check if every node has actually been used.
		for (const auto& metaData : nodeMetaDatas)
			assert(metaData.remaining == 0);

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
