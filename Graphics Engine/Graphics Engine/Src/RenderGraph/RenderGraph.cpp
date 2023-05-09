#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinearSort.h"
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
		uint32_t usesRemaining;
		uint32_t freeIndex;
	};

	struct NodeMetaData final
	{
		uint32_t inputsRemaining;
		uint32_t complexity;
		uint32_t availabilityIndex;
	};

	uint32_t CalculateDepthComplexity(const RenderGraphCreateInfo& info,
		const jv::Array<ResourceMetaData>& resourceMetaDatas, const jv::Array<NodeMetaData>& nodeMetaDatas, 
		const uint32_t index, uint32_t depth)
	{
		if (nodeMetaDatas[index].inputsRemaining > 0)
		{
			const auto& node = info.nodes[index];
			for (uint32_t i = 0; i < node.inResourceCount; ++i)
			{
				const auto& src = resourceMetaDatas[i].src;
				// Ignores out resource count, but it's relatively rare that something has multiple outputs that are used separately,
				// and it would make this function many times more expensive to write checks for that.
				depth = jv::Max(depth, CalculateDepthComplexity(info, resourceMetaDatas, nodeMetaDatas, src, depth + node.inResourceCount));
			}
		}

		nodeMetaDatas[index].complexity = depth;
		return depth;
	}

	bool ComplexitiesComparer(uint32_t& a, uint32_t& b)
	{
		return a > b;
	}

	RenderGraph RenderGraph::Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();

		uint32_t start = -1;
		for (uint32_t i = 0; i < info.nodeCount; ++i)
		{
			const auto& node = info.nodes[i];
			if(node.outResourceCount == 0)
			{
				start = i;
				break;
			}
		}
		assert(start != -1);

		// Calculate graph starting points.
		auto open = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);
		open.Add() = start;

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
			metaData.inputsRemaining = node.inResourceCount;

			for (uint32_t j = 0; j < node.inResourceCount; ++j)
				Add(tempArena, resourceMetaDatas[node.inResources[j]].dsts) = i;
			for (uint32_t j = 0; j < node.outResourceCount; ++j)
				resourceMetaDatas[node.outResources[j]].src = i;
			if (node.inResourceCount == 0)
				metaData.availabilityIndex = 0;
		}

		for (auto& metaData : resourceMetaDatas)
			metaData.usesRemaining = metaData.dsts.GetCount();

		// The ordered list of execution for when the render graph executes.
		auto ordered = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);

		while(open.count > 0)
		{
			const auto current = open[open.count - 1];
			const auto& currentMetaData = nodeMetaDatas[current];
			const auto& node = info.nodes[current];

			// If the current node is ready to be processed.
			if(currentMetaData.inputsRemaining == 0)
			{
				ordered.Add() = current;
				open.RemoveAt(open.count - 1);

				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resource = node.inResources[i];
					auto& resourceMetaData = resourceMetaDatas[resource];
					--resourceMetaData.usesRemaining;
					if (resourceMetaData.usesRemaining == 0)
						resourceMetaData.freeIndex = ordered.count;
				}

				for (uint32_t i = 0; i < node.outResourceCount; ++i)
				{
					const auto& resource = node.outResources[i];
					auto& resourceMetaData = resourceMetaDatas[resource];
					resourceMetaData.processed = true;

					for (const auto& dst : resourceMetaData.dsts)
					{
						auto& parentNodeMetaData = nodeMetaDatas[dst];
						--parentNodeMetaData.inputsRemaining;
						if (parentNodeMetaData.inputsRemaining == 0)
							parentNodeMetaData.availabilityIndex = ordered.count;
					}
				}

				continue;
			}

			// If the node has further inputs.
			if(node.inResourceCount > 0)
			{
				// Sort based on depth complexity (resource bandwidth) to find the most complex path first, to minimize resource usage.
				const auto complexitiesScope = tempArena.CreateScope();
				const auto complexities = jv::CreateArray<uint32_t>(tempArena, node.inResourceCount);
				const auto complexitiesIndices = jv::CreateArray<uint32_t>(tempArena, node.inResourceCount);

				for (uint32_t i = 0; i < complexitiesIndices.length; ++i)
					complexitiesIndices[i] = i;

				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resource = node.inResources[i];
					auto& resourceMetaData = resourceMetaDatas[resource];
					const auto& src = resourceMetaData.src;

					complexities[i] = CalculateDepthComplexity(info, resourceMetaDatas, nodeMetaDatas, src, 0);
				}

				jv::ExtLinearSort(complexities.ptr, complexitiesIndices.ptr, node.inResourceCount, ComplexitiesComparer);

				// Add the most complex child to the stack.
				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resource = node.inResources[complexitiesIndices[i]];
					auto& resourceMetaData = resourceMetaDatas[resource];
					const auto& src = resourceMetaData.src;

					if (!resourceMetaData.processed)
					{
						open.Add() = src;
						break;
					}
				}

				tempArena.DestroyScope(complexitiesScope);
			}
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
