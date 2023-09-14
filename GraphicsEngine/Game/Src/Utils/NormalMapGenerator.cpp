#include "pch_game.h"
#include "Utils/NormalMapGenerator.h"
#include "JLib/ArrayUtils.h"

namespace game
{
	jv::Array<unsigned char> GenerateNormalMap(const unsigned char* heightMap, const glm::ivec2 resolution, jv::Arena& arena)
	{
		auto normalMap = jv::CreateArray<unsigned char>(arena, resolution.x * resolution.y * 3);
		for (uint32_t y = 0; y < resolution.y; ++y)
			for (uint32_t x = 0; x < resolution.x; ++x)
			{
				// Sample general direction.
				glm::ivec3 dir{};
				dir.z = 255;
				uint32_t sideCount = 0;

				for (uint32_t y2 = 0; y2 < resolution.y; ++y2)
					for (uint32_t x2 = 0; x2 < resolution.x; ++x2)
					{
						if (x2 == 0 && y2 == 0)
							continue;

						// Check if out of bounds.
						auto coords = glm::ivec2(x + x2, y + y2);
						coords -= glm::ivec2(1);
						if (coords.x < 0 || coords.y < 0)
							continue;
						if (coords.x >= resolution.x || coords.y >= resolution.y)
							continue;

						++sideCount;

						auto dirMul = glm::ivec2(x2, y2);
						dirMul -= glm::ivec2(1);
						dirMul *= heightMap[coords.y * resolution.x + coords.x];
						dir += dirMul;
					}

				dir /= sideCount;
				glm::vec3 normDir = normalize(glm::vec3(dir));
				normDir *= 255;

				const uint32_t ind = y * resolution.x + x;
				for (uint32_t i = 0; i < 3; ++i)
					normalMap[ind + i] = static_cast<unsigned char>(normDir[i]);
			}

		return normalMap;
	}
}
