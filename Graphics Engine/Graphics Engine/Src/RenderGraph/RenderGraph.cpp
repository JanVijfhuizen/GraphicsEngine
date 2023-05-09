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
		uint32_t poolId;
	};

	struct NodeMetaData final
	{
		uint32_t inputsRemaining;
		uint32_t complexity;
		uint32_t availabilityIndex;
	};

	struct ResourcePool final
	{
		RenderGraphResourceInfo info{};
		uint32_t currentUsage = 0;
		uint32_t capacity = 0;
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

	bool RenderGraphResourceInfo::operator==(const RenderGraphResourceInfo& other) const
	{
		return other.resolution == resolution && other.type == type;
	}

	bool RenderGraphResourceInfo::operator!=(const RenderGraphResourceInfo& other) const
	{
		return !(other == *this);
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

		// Define the different resource types that are being used.
		jv::LinkedList<ResourcePool> resourceTypes{};
		for (uint32_t i = 0; i < info.resourceCount; ++i)
		{
			const auto& resource = info.resources[i];

			bool fit = false;
			uint32_t j = 0;

			for (const auto& resourceType : resourceTypes)
			{
				if (resourceType.info == resource)
				{
					fit = true;
					break;
				}

				++j;
			}

			resourceMetaDatas[i].poolId = j;
			if (fit)
				continue;
			auto& pool = Add(tempArena, resourceTypes) = {};
			pool.info = resource;
		}

		const auto resourcePool = ToArray(tempArena, resourceTypes, true);
		const auto poolStates = jv::CreateArray<jv::Array<uint32_t>>(tempArena, ordered.length);

		// Calculate minimum pool capacities.
		{
			uint32_t j = 0;
			for (const auto& index : ordered)
			{
				const auto& node = info.nodes[index];
				for (uint32_t i = 0; i < node.outResourceCount; ++i)
				{
					const auto& resourceMetaData = resourceMetaDatas[node.outResources[i]];
					auto& pool = resourcePool[resourceMetaData.poolId];
					++pool.currentUsage;
					if (pool.currentUsage > pool.capacity)
						pool.capacity = pool.currentUsage;
				}

				auto& poolState = poolStates[j] = jv::CreateArray<uint32_t>(tempArena, resourcePool.length);
				for (uint32_t i = 0; i < resourcePool.length; ++i)
					poolState[i] = resourcePool[i].currentUsage;

				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resourceMetaData = resourceMetaDatas[node.inResources[i]];
					auto& pool = resourcePool[resourceMetaData.poolId];
					if (resourceMetaData.freeIndex == j + 1)
						--pool.currentUsage;
				}

				++j;
			}
		}

		// Calculate batches.
		jv::LinkedList<jv::LinkedList<uint32_t>> batches{};

		const auto batched = jv::CreateArray<bool>(tempArena, ordered.length);
		for (auto& b : batched)
			b = false;
		const auto maximumPoolState = jv::CreateArray<uint32_t>(tempArena, resourcePool.length);
		for (uint32_t i = 0; i < ordered.length; ++i)
		{
			if (batched[i])
				continue;

			auto& batch = Add(tempArena, batches) = {};
			Add(tempArena, batch) = i;
			auto& poolState = poolStates[i];
			for (uint32_t j = 0; j < resourcePool.length; ++j)
				maximumPoolState[j] = poolState[j];

			for (uint32_t j = i + 1; j < ordered.length; ++j)
			{
				if (batched[j])
					continue;

				auto& otherPoolState = poolStates[j];
				for (uint32_t k = 0; k < resourcePool.length; ++k)
					maximumPoolState[k] = jv::Max(maximumPoolState[k], otherPoolState[k]);

				const auto& nodeMetaData = nodeMetaDatas[ordered[j]];
				if(!batched[j] && nodeMetaData.availabilityIndex <= i)
				{
					bool fit = true;
					for (uint32_t k = 0; k < resourcePool.length; ++k)
						if(maximumPoolState[k] + otherPoolState[k] > resourcePool[k].capacity)
						{
							fit = false;
							break;
						}

					if(fit)
					{
						for (uint32_t k = 0; k < resourcePool.length; ++k)
							maximumPoolState[k] += otherPoolState[k];
						Add(tempArena, batch) = j;
						batched[j] = true;
					}
				}
			}
		}

		for (const auto& batch : batches)
		{
			std::cout << "batch: ";
			for (const auto& i : batch)
			{
				std::cout << ordered[i] << ", ";
			}

			std::cout << std::endl;
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
