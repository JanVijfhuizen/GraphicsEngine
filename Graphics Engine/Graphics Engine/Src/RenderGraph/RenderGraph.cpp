#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedList.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/VectorUtils.h"

namespace ge
{
	struct ResourceMetaData final
	{
		uint32_t src = 0;
		jv::LinkedList<uint32_t> dsts{};
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
		const auto contained = jv::CreateArray<bool>(tempArena, info.nodeCount);
		for (auto& b : contained)
			b = false;
		contained[leafs[0]] = true;

		while(open.count > 0)
		{
			const auto current = open[open.count - 1];
			auto& currentMetaData = nodeMetaDatas[current];
			const auto& node = info.nodes[current];

			if(currentMetaData.remaining == 0)
			{
				std::cout << "h" << std::endl;
				for (unsigned ordered1 : ordered)
				{
					std::cout << ordered1 << std::endl;
				}

				ordered.Add() = current;
				open.RemoveAt(open.count - 1);

				bool parentContained = false;
				for (uint32_t i = 0; i < node.outResourceCount; ++i)
				{
					const auto& resource = node.outResources[i];
					const auto& resourceMetaData = resourceMetaDatas[resource];

					for (const auto& dst : resourceMetaData.dsts)
					{
						--nodeMetaDatas[dst].remaining;
						if (contained[dst])
						{
							parentContained = true;
							break;
						}
					}

					if (parentContained)
						break;

					for (const auto& dst : resourceMetaData.dsts)
					{
						auto& dstContained = contained[dst];
						open.Add() = dst;
						dstContained = true;
					}
				}

				continue;
			}

			for (uint32_t i = 0; i < node.inResourceCount; ++i)
			{
				const auto& resource = node.inResources[i];
				const auto& resourceMetaData = resourceMetaDatas[resource];
				const auto& src = resourceMetaData.src;

				auto& srcContained = contained[src];
				if (srcContained)
					continue;
				open.Add() = src;
				srcContained = true;
			}
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
