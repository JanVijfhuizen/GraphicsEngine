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
		bool satisfies;
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

	float UpdateSatisfaction(ResourceMetaData* resourceMetaDatas, bool* visited, bool* executed,
		const uint32_t current, const RenderGraphCreateInfo& info, bool* outSatisfies)
	{
		if (visited[current])
			return 0;
		visited[current] = true;

		const auto& node = info.nodes[current];

		auto satisfaction = -static_cast<float>(node.outResourceCount);

		for (uint32_t j = 0; j < node.inResourceCount; ++j)
		{
			const auto& resourceMetaData = resourceMetaDatas[node.inResources[j]];
			satisfaction += 1.f / static_cast<float>(resourceMetaData.dstsCount);
			if (executed[resourceMetaData.src])
			{
				*outSatisfies = true;
				continue;
			}

			bool satisfies = false;
			satisfaction += UpdateSatisfaction(resourceMetaDatas, visited, executed, resourceMetaData.src, info, &satisfies);
		}

		return satisfaction;
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

	jv::Vector<uint32_t> FindPath(jv::Arena& tempArena, NodeMetaData* nodeMetaDatas, ResourceMetaData* resourceMetaDatas, const uint32_t root, const RenderGraphCreateInfo& info)
	{
		auto executeOrder = jv::CreateVector<uint32_t>(tempArena, info.nodeCount);
		const auto tempScope = tempArena.CreateScope();
		const auto executed = jv::CreateArray<bool>(tempArena, info.nodeCount);
		for (auto& b : executed)
			b = false;

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

				bool satisfies = false;
				const float satisfaction = UpdateSatisfaction(resourceMetaDatas, visited.ptr, executed.ptr, i, info, &satisfies);
				metaData.satisfaction = satisfaction;
				metaData.satisfies = satisfies;

				tempArena.DestroyScope(tempLoopScope);
			}

			// If no positive nodes have been found, execute the least satisfying node.
			uint32_t current = root;
			while(true)
			{
				const auto& node = info.nodes[current];

				uint32_t optimalChild = -1;
				float optimalChildSatisfaction = -FLT_EPSILON;
				bool optimalChildSatisfies = false;

				for (uint32_t i = 0; i < node.inResourceCount; ++i)
				{
					const auto& resourceMetaData = resourceMetaDatas[node.inResources[i]];
					const uint32_t src = resourceMetaData.src;
					if (executed[src])
						continue;

					const auto& nodeMetaData = nodeMetaDatas[src];
					const float satisfaction = nodeMetaData.satisfaction;

					if (optimalChildSatisfies && satisfaction < 0)
						continue;

					// If a more positive node is found.

					const bool satisfactionCheck = nodeMetaData.satisfies && !optimalChildSatisfies;
					const bool positiveCheck = nodeMetaData.satisfaction >= optimalChildSatisfaction;

					if(satisfactionCheck || optimalChildSatisfies && positiveCheck || !optimalChildSatisfies && !positiveCheck)
					{
						optimalChild = src;
						optimalChildSatisfaction = satisfaction;
						optimalChildSatisfies = nodeMetaData.satisfies;
					}
				}

				if (optimalChild != -1)
				{
					current = optimalChild;
					continue;
				}

				executeOrder.Add() = current;
				executed[current] = true;
				break;
			}
		}

		tempArena.DestroyScope(tempScope);
		return executeOrder;
	}

	jv::Vector<Pool> DefinePools(jv::Arena& tempArena, NodeMetaData* nodeMetaDatas, ResourceMetaData* resourceMetaDatas, 
		const jv::Vector<uint32_t>& path, const RenderGraphCreateInfo& info)
	{
		auto pools = jv::CreateVector<Pool>(tempArena, info.resourceCount);
		const auto tempScope = tempArena.CreateScope();
		const auto poolIndices = jv::CreateArray<uint32_t>(tempArena, info.resourceCount);

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
				const auto& pool = pools[poolIndex];
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
		return pools;
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
		const auto pools = DefinePools(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, path, info);

		for (unsigned node : path)
		{
			std::cout << node << std::endl;
		}

		std::cout << std::endl;

		for (auto& pool : pools)
		{
			std::cout << pool.capacity << std::endl;
		}

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
