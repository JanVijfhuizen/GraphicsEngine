﻿#include "pch.h"
#include "Jlib/PackingFFDH.h"

#include "JLib/ArrayUtils.h"
#include "JLib/LinearSort.h"
#include "JLib/VectorUtils.h"

namespace jv
{
	struct Ref final
	{
		glm::ivec2 shape;
		float length;
		uint32_t index;
	};

	bool RefSorter(Ref& a, Ref& b)
	{
		return a.length > b.length;
	}

	Array<glm::ivec2> Pack(Arena& arena, Arena& tempArena, const Array<glm::ivec2>& shapes, glm::ivec2& outArea)
	{
		const auto _ = tempArena.CreateScope();

		struct Node final
		{
			glm::ivec2 position{};
			glm::ivec2 shape{};
		};

		// Collect metadata from the shapes.
		const uint32_t length = shapes.length;

		const auto refs = CreateArray<Ref>(tempArena, length);
		for (uint32_t i = 0; i < length; ++i)
		{
			auto& ref = refs[i];
			ref.shape = shapes[i];
			glm::vec2 l = { ref.shape.x, ref.shape.y };
			ref.length = glm::length(l);
			ref.index = i;
			assert(ref.shape.x > 0 && ref.shape.y > 0);
		}

		// Sort from largest to smallest.
		LinearSort(refs.ptr, length, RefSorter);

		glm::ivec2 area{ 32 };
		while (true)
		{
			const auto _ = tempArena.CreateScope();

			auto nodes = CreateVector<Node>(tempArena, length * 2 + 1);

			{
				auto& root = nodes.Add();
				root.shape = area;
				assert(area.x > 0 && area.y > 0);
			}

			Vector<glm::ivec2> ret = CreateVector<glm::ivec2>(tempArena, length);

			bool fit = true;

			// Iterate over all the references and put them in nodes.
			for (const auto& ref : refs)
			{
				const uint32_t nodeCount = nodes.count;
				const auto& shape = ref.shape;

				fit = false;
				for (uint32_t i = 0; i < nodeCount; ++i)
				{
					const auto& node = nodes[i];
					const auto& nodeShape = node.shape;

					const auto diff = glm::ivec2(nodeShape.x - shape.x, nodeShape.y - shape.y);

					// If it does not fit within the node.
					if (diff.x < 0 || diff.y < 0)
						continue;

					ret.Add() = node.position;

					if (diff.y > 0)
					{
						auto& newNode = nodes.Add();
						newNode.position = node.position + glm::ivec2(0, shape.y);
						newNode.shape = node.shape - glm::ivec2(0, shape.y);
					}
					if (diff.x > 0)
					{
						auto& newNode = nodes.Add();
						newNode.position = node.position + glm::ivec2(shape.x, 0);
						newNode.shape = node.shape - glm::ivec2(shape.x, diff.y);
					}

					nodes.RemoveAt(i);
					fit = true;
					break;
				}

				if (!fit)
					break;
			}

			if (!fit)
			{
				area *= 2;
				continue;
			}

			const auto result = CreateArray<glm::ivec2>(arena, ret.length);
			for (uint32_t i = 0; i < ret.length; ++i)
				result[refs[i].index] = ret[i];
			outArea = area;
			return result;
		}
	}
}
