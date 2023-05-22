#include "pch.h"
#include "RenderGraph/RenderGraph.h"
#include "JLib/ArrayUtils.h"
#include "JLib/LinkedListUtils.h"
#include "JLib/VectorUtils.h"

namespace ge
{
	struct NodeMetaData final
	{
		float satisfaction = 0;
		float complexity = 0;
	};

	struct ResourceMetaData final
	{
		uint32_t src;
		jv::LinkedList<uint32_t> dsts{};
		uint32_t dstsCount;
	};

	struct Pool final
	{
		ResourceMaskDescription mask;
		uint32_t capacity;
	};

	struct DefinedPools final
	{
		jv::Array<Pool> pools{};
		jv::Array<uint32_t> poolIndices{};
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

	void DefineResourceMetaData(jv::Arena& arena, ResourceMetaData* metaDatas, const RenderGraphCreateInfo& info)
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

	jv::Vector<uint32_t> FindPath(jv::Arena& tempArena, NodeMetaData* nodeMetaDatas, 
		ResourceMetaData* resourceMetaDatas, const uint32_t root, const RenderGraphCreateInfo& info)
	{
		auto executeOrder = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);
		const auto tempScope = tempArena.CreateScope();
		const auto executed = jv::CreateArray<bool>(tempArena, info.nodeCount);
		for (auto& b : executed)
			b = false;
		const auto resourceUsages = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceUsages[i] = resourceMetaDatas[i].dstsCount;

		while(!executed[root])
		{
			// Update satisfaction.
			for (uint32_t i = 0; i < info.nodeCount; ++i)
			{
				auto& metaData = nodeMetaDatas[i];

				const auto tempLoopScope = tempArena.CreateScope();
				const auto visited = jv::CreateArray<bool>(tempArena, info.nodeCount);
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

	DefinedPools DefinePools(jv::Arena& tempArena, const NodeMetaData* nodeMetaDatas, const ResourceMetaData* resourceMetaDatas,
		const jv::Vector<uint32_t>& path, const RenderGraphCreateInfo& info)
	{
		auto pools = jv::CreateVector<Pool>(tempArena, info.resourceCount);
		const auto poolIndices = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);
		const auto tempScope = tempArena.CreateScope();

		// Define all resource types.
		for (uint32_t i = 0; i < info.resourceCount; ++i)
		{
			const auto& resource = info.resources[i];

			uint32_t poolIndex = pools.count;
			for (uint32_t j = 0; j < pools.count; ++j)
			{
				if (pools[j].mask == resource)
				{
					poolIndex = j;
					break;
				}
			}

			poolIndices[i] = poolIndex;
			if (poolIndex != pools.count)
				continue;

			auto& pool = pools.Add() = {};
			pool.mask = resource;
		}

		const auto poolUsage = jv::CreateArray<uint32_t>(tempArena, pools.count);
		const auto resourceUsagesRemaining = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);
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

	jv::Vector<uint32_t> OptimizePath(jv::Arena& tempArena, const NodeMetaData* nodeMetaDatas, const ResourceMetaData* resourceMetaDatas,
		const jv::Vector<uint32_t>& path, const DefinedPools& pools, const RenderGraphCreateInfo& info)
	{
		auto optimizedPath = jv::CreateVector<uint32_t>(tempArena, path.length);
		auto optimizedPathIndices = jv::CreateVector<uint32_t>(tempArena, path.length);

		const auto tempScope = tempArena.CreateScope();

		// Track pool capacity.
		const auto poolsCapacity = jv::CreateArray<uint32_t>(tempArena, pools.pools.length);
		const auto poolsCurrentCapacity = jv::CreateArray<uint32_t>(tempArena, pools.pools.length);
		for (uint32_t i = 0; i < poolsCurrentCapacity.length; ++i)
			poolsCurrentCapacity[i] = pools.pools[i].capacity;

		// Track resource usage.
		const auto resourceUsagesRemaining = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);
		const auto resourceCurrentUsagesRemaining = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);
		for (uint32_t i = 0; i < info.resourceCount; ++i)
			resourceCurrentUsagesRemaining[i] = resourceMetaDatas[i].dstsCount;
		const auto resourcesReady = jv::CreateArray<bool>(tempArena, info.resourceCount);
		for (auto& ready : resourcesReady)
			ready = false;

		const auto poolsMinCapacity = jv::CreateArray<uint32_t>(tempArena, pools.pools.length);

		auto open = jv::CreateVector<uint32_t>(tempArena, path.count);
		open.count = path.count;
		for (uint32_t i = 0; i < path.count; ++i)
			open[i] = path[i];
		auto batch = jv::CreateVector<uint32_t>(tempArena, path.count);

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

		for (unsigned& batch1 : optimizedPath)
		{
			std::cout << batch1 << std::endl;
		}

		std::cout << std::endl;

		for (unsigned& batch1 : optimizedPathIndices)
		{
			std::cout << batch1 << std::endl;
		}

		tempArena.DestroyScope(tempScope);
		return {};
	}

	RenderGraph RenderGraph::Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();
		
		const auto nodeMetaDatas = jv::CreateArray<NodeMetaData>(tempArena, info.nodeCount);
		const auto resourceMetaDatas = jv::CreateArray<ResourceMetaData>(tempArena, info.resourceCount);

		DefineNodeMetaData(nodeMetaDatas.ptr, info);
		DefineResourceMetaData(tempArena, resourceMetaDatas.ptr, info);
		const uint32_t root = FindRoot(info);

		const auto path = FindPath(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, root, info);
		const auto definedPools = DefinePools(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, path, info);

		for (unsigned node : path)
		{
			std::cout << node << std::endl;
		}

		std::cout << std::endl;

		const auto optimizedPath = OptimizePath(tempArena, 
			nodeMetaDatas.ptr, resourceMetaDatas.ptr, path, definedPools, info);

		for (unsigned node : optimizedPath)
		{
			std::cout << node << std::endl;
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
