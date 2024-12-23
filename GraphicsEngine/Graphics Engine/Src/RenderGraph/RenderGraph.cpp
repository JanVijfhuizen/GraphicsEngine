#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/VectorUtils.h"

namespace jv::rg
{
	struct NodeMetaData final
	{
		float satisfaction = 0;
		float complexity = 0;
	};

	struct ResourceMetaData final
	{
		uint32_t src;
		LinkedList<uint32_t> dsts{};
		uint32_t dstsCount;
	};

	struct DefinedPools final
	{
		Array<RenderGraph::Pool> pools{};
		Array<uint32_t> poolIndices{};
	};

	struct DefinedBatches final
	{
		Array<uint32_t> nodeIndices{};
		Array<uint32_t> batchIndices{};
	};

	struct DefinedAllocations final
	{
		LinkedList<RenderGraphResource> allocations{};
		LinkedList<RenderGraphResource> deallocations{};
	};

	void UpdateNodeMetaData(ResourceMetaData* resourceMetaDatas, uint32_t* resourceUsages, bool* visited, bool* executed,
		const uint32_t current, const RenderGraphCreateInfo& info, float* outSatisfaction, float* outComplexity)
	{
		if (visited[current])
			return;
		visited[current] = true;

		const auto& node = info.nodes[current];

		auto satisfaction = -static_cast<float>(node.outResourceCount);
		auto complexity = static_cast<float>(node.inResourceCount + node.outResourceCount);

		for (uint32_t j = 0; j < node.inResourceCount; ++j)
		{
			const auto& resourceMetaData = resourceMetaDatas[node.inResources[j]];
			satisfaction += 1.f / static_cast<float>(resourceUsages[j]);
			if (executed[resourceMetaData.src])
				continue;
			
			UpdateNodeMetaData(resourceMetaDatas, resourceUsages, visited, executed, resourceMetaData.src, info, &satisfaction, &complexity);
		}

		*outSatisfaction = satisfaction;
		*outComplexity = complexity;
	}

	void DefineNodeMetaData(NodeMetaData* metaDatas, const RenderGraphCreateInfo& info)
	{
		for (uint32_t i = 0; i < info.nodeCount; ++i)
			metaDatas[i] = {};
	}

	void DefineResourceMetaData(Arena& arena, ResourceMetaData* metaDatas, const RenderGraphCreateInfo& info)
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

		for (uint32_t i = 0; i < info.resourceCount; ++i)
		{
			auto& metaData = metaDatas[i];
			metaData.dstsCount = metaData.dsts.GetCount();
		}
	}

	uint32_t FindRoot(const RenderGraphCreateInfo& info)
	{
		uint32_t root = -1;
		for (uint32_t i = 0; i < info.nodeCount; ++i)
			root = info.nodes[i].outResourceCount == 0 ? i : root;
		return root;
	}

	Vector<uint32_t> FindPath(Arena& tempArena, NodeMetaData* nodeMetaDatas, 
		ResourceMetaData* resourceMetaDatas, const uint32_t root, const RenderGraphCreateInfo& info)
	{
		auto executeOrder = CreateVector<uint32_t>(tempArena, info.nodeCount);
		const auto tempScope = tempArena.CreateScope();
		const auto executed = CreateArray<bool>(tempArena, info.nodeCount);
		for (auto& b : executed)
			b = false;
		const auto resourceUsages = CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceUsages[i] = resourceMetaDatas[i].dstsCount;

		while(!executed[root])
		{
			// Update satisfaction.
			for (uint32_t i = 0; i < info.nodeCount; ++i)
			{
				auto& metaData = nodeMetaDatas[i];

				const auto tempLoopScope = tempArena.CreateScope();
				const auto visited = CreateArray<bool>(tempArena, info.nodeCount);
				for (auto& b : visited)
					b = false;

				UpdateNodeMetaData(resourceMetaDatas, resourceUsages.ptr, visited.ptr, executed.ptr, i, info,
					&metaData.satisfaction, &metaData.complexity);

				tempArena.DestroyScope(tempLoopScope);
			}

			// If no positive nodes have been found, execute the least satisfying node.
			uint32_t current = root;
			while(true)
			{
				const auto& node = info.nodes[current];

				uint32_t optimalChild = -1;
				float optimalChildSatisfaction = -FLT_MAX;
				float optimalChildComplexity = 0;

				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resourceMetaData = resourceMetaDatas[node.inResources[i]];
					const uint32_t src = resourceMetaData.src;
					if (executed[src])
						continue;

					const auto& nodeMetaData = nodeMetaDatas[src];

					const bool equal = fabs(nodeMetaData.satisfaction - optimalChildSatisfaction) < FLT_EPSILON;
					bool valid = false;
					if (equal && nodeMetaDatas->complexity > optimalChildComplexity)
						valid = true;
					if (!equal && nodeMetaData.satisfaction > optimalChildSatisfaction)
						valid = true;

					if (!valid)
						continue;

					optimalChild = src;
					optimalChildSatisfaction = nodeMetaData.satisfaction;
					optimalChildComplexity = nodeMetaData.complexity;
				}

				if (optimalChild != -1)
				{
					current = optimalChild;
					continue;
				}

				executeOrder.Add() = current;
				executed[current] = true;
				
				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const uint32_t resourceIndex = node.inResources[i];
					--resourceUsages[resourceIndex];
				}

				break;
			}
		}

		tempArena.DestroyScope(tempScope);
		return executeOrder;
	}

	DefinedPools DefinePools(Arena& tempArena, const NodeMetaData* nodeMetaDatas, const ResourceMetaData* resourceMetaDatas,
		const Vector<uint32_t>& path, const RenderGraphCreateInfo& info)
	{
		auto pools = CreateVector<RenderGraph::Pool>(tempArena, info.resourceCount);
		const auto poolIndices = CreateArray<uint32_t>(tempArena, info.resourceCount);
		const auto tempScope = tempArena.CreateScope();

		// Define all resource types.
		for (uint32_t i = 0; i < info.resourceCount; ++i)
		{
			const auto& resource = info.resources[i];

			uint32_t poolIndex = pools.count;
			for (uint32_t j = 0; j < pools.count; ++j)
			{
				if (pools[j].resourceMaskDescription == resource)
				{
					poolIndex = j;
					break;
				}
			}

			poolIndices[i] = poolIndex;
			if (poolIndex != pools.count)
				continue;

			auto& pool = pools.Add() = {};
			pool.resourceMaskDescription = resource;
		}

		const auto poolUsage = CreateArray<uint32_t>(tempArena, pools.count);
		const auto resourceUsagesRemaining = CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (auto& usage : poolUsage)
			usage = 0;
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceUsagesRemaining[i] = resourceMetaDatas[i].dstsCount;

		for (const auto& nodeIndex : path)
		{
			const auto& node = info.nodes[nodeIndex];
			for (uint32_t i = 0; i < node.inResourceCount; ++i)
			{
				const auto resourceIndex = node.inResources[i];
				const auto poolIndex = poolIndices[resourceIndex];
				if (--resourceUsagesRemaining[resourceIndex] == 0)
					--poolUsage[poolIndex];
			}
			for (uint32_t i = 0; i < node.outResourceCount; ++i)
			{
				const auto resourceIndex = node.outResources[i];
				const auto poolIndex = poolIndices[resourceIndex];
				auto& pool = pools[poolIndex];

				if (++poolUsage[poolIndex] > pool.capacity)
					pool.capacity = poolUsage[poolIndex];
			}
		}

		tempArena.DestroyScope(tempScope);

		DefinedPools definedPools{};
		definedPools.pools.ptr = pools.ptr;
		definedPools.pools.length = pools.count;
		definedPools.poolIndices = poolIndices;
		return definedPools;
	}

	DefinedBatches DefineBatches(Arena& tempArena, const NodeMetaData* nodeMetaDatas, const ResourceMetaData* resourceMetaDatas,
		const Vector<uint32_t>& path, const DefinedPools& pools, const RenderGraphCreateInfo& info)
	{
		auto optimizedPath = CreateVector<uint32_t>(tempArena, path.length);
		auto optimizedPathIndices = CreateVector<uint32_t>(tempArena, path.length);

		const auto tempScope = tempArena.CreateScope();

		// Track pool capacity.
		const auto poolsCapacity = CreateArray<uint32_t>(tempArena, pools.pools.length);
		const auto poolsCurrentCapacity = CreateArray<uint32_t>(tempArena, pools.pools.length);
		for (uint32_t i = 0; i < poolsCurrentCapacity.length; ++i)
			poolsCurrentCapacity[i] = pools.pools[i].capacity;

		// Track resource usage.
		const auto resourceUsagesRemaining = CreateArray<uint32_t>(tempArena, info.resourceCount);
		const auto resourceCurrentUsagesRemaining = CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceCurrentUsagesRemaining[i] = resourceMetaDatas[i].dstsCount;
		const auto resourcesReady = CreateArray<bool>(tempArena, info.resourceCount);
		for (auto& ready : resourcesReady)
			ready = false;

		const auto poolsMinCapacity = CreateArray<uint32_t>(tempArena, pools.pools.length);
		auto open = CreateVector<uint32_t>(tempArena, path.count);
		open.count = path.count;
		for (uint32_t i = 0; i < path.count; ++i)
			open[i] = path[i];
		auto batch = CreateVector<uint32_t>(tempArena, path.count);

		while(open.count > 0)
		{
			// Update pool capacity.
			for (uint32_t i = 0; i < poolsCapacity.length; ++i)
			{
				poolsCapacity[i] = poolsCurrentCapacity[i];
				poolsMinCapacity[i] = poolsCurrentCapacity[i];
			}

			for (uint32_t i = 0; i < resourceUsagesRemaining.length; ++i)
				resourceUsagesRemaining[i] = resourceCurrentUsagesRemaining[i];

			// Try to batch nodes.
			for (uint32_t i = 0; i < open.count; ++i)
			{
				const uint32_t nodeIndex = open[i];
				const auto& node = info.nodes[nodeIndex];

				// Check for available capacity.
				bool fit = true;
				for (uint32_t j = 0; j < node.outResourceCount; ++j)
				{
					const uint32_t resourceIndex = node.outResources[j];
					const uint32_t poolIndex = pools.poolIndices[resourceIndex];
					auto& capacity = poolsCapacity[poolIndex];
					auto& minCapacity = poolsMinCapacity[poolIndex];

					if (minCapacity == 0)
					{
						fit = false;
						break;
					}

					// Decrease pool remaining capacity.
					--capacity;
					if (capacity < minCapacity)
						minCapacity = capacity;
				}

				if (!fit)
					break;

				// Check if leaf.
				bool isLeaf = true;
				for (uint32_t j = 0; j < node.inResourceCount; ++j)
					if (!resourcesReady[node.inResources[j]])
					{
						isLeaf = false;
						break;
					}

				// Increase pool remaining capacity.
				if (!isLeaf)
				{
					for (uint32_t j = 0; j < node.inResourceCount; ++j)
					{
						const uint32_t resourceIndex = node.inResources[j];
						const uint32_t poolIndex = pools.poolIndices[resourceIndex];
						if (--resourceUsagesRemaining[resourceIndex] == 0)
							++poolsCapacity[poolIndex];
					}
				}

				// Add if leaf.
				if (isLeaf)
					batch.Add() = i;
			}

			// Remove nodes that have now been traveled to.
			for (int32_t i = static_cast<int32_t>(batch.count) - 1; i >= 0; --i)
			{
				const uint32_t openIndex = batch[i];
				const uint32_t nodeIndex = open[openIndex];
				open.RemoveAtOrdered(openIndex);
				optimizedPath.Add() = nodeIndex;

				const auto& node = info.nodes[nodeIndex];

				for (uint32_t j = 0; j < node.inResourceCount; ++j)
				{
					const uint32_t resourceIndex = node.inResources[j];
					const uint32_t poolIndex = pools.poolIndices[resourceIndex];
					if (--resourceCurrentUsagesRemaining[resourceIndex] == 0)
						++poolsCurrentCapacity[poolIndex];
				}

				for (uint32_t j = 0; j < node.outResourceCount; ++j)
				{
					const uint32_t resourceIndex = node.outResources[j];
					const uint32_t poolIndex = pools.poolIndices[resourceIndex];
					--poolsCurrentCapacity[poolIndex];
					resourcesReady[resourceIndex] = true;
				}
			}

			optimizedPathIndices.Add() = path.count - open.count;
			batch.Clear();
		}

		tempArena.DestroyScope(tempScope);

		DefinedBatches definedBatches{};
		definedBatches.nodeIndices.ptr = optimizedPath.ptr;
		definedBatches.nodeIndices.length = optimizedPath.count;
		definedBatches.batchIndices.ptr = optimizedPathIndices.ptr;
		definedBatches.batchIndices.length = optimizedPathIndices.count;
		return definedBatches;
	}

	Array<DefinedAllocations> DefineAllocations(Arena& tempArena, const ResourceMetaData* resourceMetaDatas, const DefinedPools& pools, 
		const DefinedBatches& batches, const RenderGraphCreateInfo& info)
	{
		const auto allocations = CreateArray<DefinedAllocations>(tempArena, batches.nodeIndices.length);
		const auto resourceUsagesRemaining = CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceUsagesRemaining[i] = resourceMetaDatas[i].dstsCount;

		const auto instancePools = CreateArray<Vector<uint32_t>>(tempArena, pools.pools.length);
		for (uint32_t i = 0; i < instancePools.length; ++i)
		{
			auto& instancePool = instancePools[i] = CreateVector<uint32_t>(tempArena, pools.pools[i].capacity);
			instancePool.count = instancePool.length;
			for (uint32_t j = 0; j < instancePool.length; ++j)
				instancePool[j] = j;
		}

		const auto assignedInstances = CreateArray<LinkedList<RenderGraphResource>>(
			tempArena, info.nodeCount);
		for (auto& assignedInstance : assignedInstances)
			assignedInstance = {};

		uint32_t current = 0;
		for (uint32_t i = 0; i < batches.batchIndices.length; ++i)
		{
			const uint32_t batchIndex = batches.batchIndices[i];

			uint32_t prevCurrent = current;
			while (current < batchIndex)
			{
				auto& definedAllocation = allocations[current] = {};

				const uint32_t nodeIndex = batches.nodeIndices[current];
				const auto& node = info.nodes[nodeIndex];

				for (uint32_t j = 0; j < node.outResourceCount; ++j)
				{
					const uint32_t resourceIndex = node.outResources[j];
					const uint32_t poolIndex = pools.poolIndices[resourceIndex];
					RenderGraphResource allocation{};
					allocation.pool = poolIndex;
					allocation.instance = instancePools[poolIndex].Pop();
					Add(tempArena, definedAllocation.allocations) = allocation;

					auto& resourceMetaData = resourceMetaDatas[resourceIndex];
					for (const auto& dst : resourceMetaData.dsts)
						Add(tempArena, assignedInstances[dst]) = allocation;
				}

				++current;
			}

			while (prevCurrent < batchIndex)
			{
				auto& definedAllocation = allocations[prevCurrent];
				const uint32_t nodeIndex = batches.nodeIndices[prevCurrent];
				const auto& node = info.nodes[nodeIndex];

				for (uint32_t j = 0; j < node.inResourceCount; ++j)
				{
					const uint32_t resourceIndex = node.inResources[j];
					const auto& poolInstance = assignedInstances[nodeIndex][j];
					Add(tempArena, definedAllocation.deallocations) = poolInstance;
					if (--resourceUsagesRemaining[resourceIndex] > 0)
						continue;

					instancePools[poolInstance.pool].Add() = poolInstance.instance;
				}

				++prevCurrent;
			}
		}

		return allocations;
	}

	RenderGraph RenderGraph::Create(Arena& arena, Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();
		
		const auto nodeMetaDatas = CreateArray<NodeMetaData>(tempArena, info.nodeCount);
		const auto resourceMetaDatas = CreateArray<ResourceMetaData>(tempArena, info.resourceCount);

		DefineNodeMetaData(nodeMetaDatas.ptr, info);
		DefineResourceMetaData(tempArena, resourceMetaDatas.ptr, info);
		const uint32_t root = FindRoot(info);

		const auto path = FindPath(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, root, info);
		const auto definedPools = DefinePools(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, path, info);
		const auto definedBatches = DefineBatches(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, path, definedPools, info);

		graph.scope = arena.CreateScope();
		graph.pools = CreateArray<Pool>(arena, definedPools.pools.length);
		for (uint32_t i = 0; i < graph.pools.length; ++i)
			graph.pools[i] = definedPools.pools[i];
		graph.batches = CreateArray<Batch>(arena, definedBatches.batchIndices.length);

		const auto allocations = DefineAllocations(tempArena, 
			resourceMetaDatas.ptr, definedPools, definedBatches, info);

		uint32_t i = 0;
		for (uint32_t j = 0; j < graph.batches.length; ++j)
		{
			auto& batch = graph.batches[j] = {};
			const uint32_t batchIndex = definedBatches.batchIndices[j];
			batch.passes = CreateArray<Pass>(arena, batchIndex - i);

			for (uint32_t k = 0; k < batch.passes.length; ++k)
			{
				auto& pass = batch.passes[k] = {};
				pass.nodeIndex = definedBatches.nodeIndices[i + k];
				const auto& node = info.nodes[pass.nodeIndex];

				pass.inResources = ToArray(arena, allocations[i + k].deallocations, false);
				pass.outResources = ToArray(arena, allocations[i + k].allocations, false);
			}

			i = batchIndex;
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Debug() const
	{
		for (auto& batch : batches)
		{
			std::cout << "batch: " << std::endl;
			for (auto& pass : batch.passes)
			{
				std::cout << "node: " << pass.nodeIndex << std::endl;
				std::cout << "--in--: " << std::endl;
				for (const auto& resource : pass.inResources)
					std::cout << resource.pool << " " << resource.instance << std::endl;
				std::cout << "--out--: " << std::endl;
				for (const auto& resource : pass.outResources)
					std::cout << resource.pool << " " << resource.instance << std::endl;
			}
		}
	}

	void RenderGraph::Destroy(Arena& arena, const RenderGraph& renderGraph)
	{
		arena.DestroyScope(renderGraph.scope);
	}
}
