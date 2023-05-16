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

	bool RenderGraphResourceInfo::operator==(const RenderGraphResourceInfo& other) const
	{
		return other.resolution == resolution && other.type == type;
	}

	bool RenderGraphResourceInfo::operator!=(const RenderGraphResourceInfo& other) const
	{
		return !(other == *this);
	}

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

	void DefineNodeMetaData(jv::Arena& tempArena, NodeMetaData* metaDatas, ResourceMetaData* resourceMetaDatas, const RenderGraphCreateInfo& info)
	{
		for (uint32_t i = 0; i < info.nodeCount; ++i)
			metaDatas[i] = {};
	}

	void FindPath(jv::Arena& tempArena, NodeMetaData* nodeMetaDatas, ResourceMetaData* resourceMetaDatas, const uint32_t root, const RenderGraphCreateInfo& info)
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

		for (unsigned order : executeOrder)
		{
			std::cout << order << std::endl;
		}

		tempArena.DestroyScope(tempScope);
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

	RenderGraph RenderGraph::Create(jv::Arena& arena, jv::Arena& tempArena, const RenderGraphCreateInfo& info)
	{
		RenderGraph graph{};
		const auto tempScope = tempArena.CreateScope();
		
		const auto nodeMetaDatas = jv::CreateArray<NodeMetaData>(tempArena, info.nodeCount);
		const auto resourceMetaDatas = jv::CreateArray<ResourceMetaData>(tempArena, info.resourceCount);

		DefineResourceMetaData(tempArena, resourceMetaDatas.ptr, info);
		DefineNodeMetaData(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, info);
		const uint32_t root = FindRoot(info);

		/*
		for (int i = 0; i < info.nodeCount; ++i)
		{
			std::cout << nodeMetaDatas[i].satisfaction << std::endl;
		}
		*/

		FindPath(tempArena, nodeMetaDatas.ptr, resourceMetaDatas.ptr, root, info);

		tempArena.DestroyScope(tempScope);
		return graph;
	}

	void RenderGraph::Destroy(jv::Arena& arena, const RenderGraph& renderGraph)
	{
	}
}
