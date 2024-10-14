#include "pch_game.h"
#include "Utils/SubTextureUtils.h"
#include "GE/SubTexture.h"

namespace game
{
	void Divide(const jv::ge::SubTexture& subTexture, jv::ge::SubTexture* arr, const uint32_t length)
	{
		const float partition = (subTexture.rBot.x - subTexture.lTop.x) / static_cast<float>(length);
		for (uint32_t i = 0; i < length; ++i)
		{
			auto& sub = arr[i] = subTexture;
			sub.lTop.x += partition * static_cast<float>(i);
			sub.rBot.x = sub.lTop.x + partition;
		}
	}

	void Divide2d(const jv::ge::SubTexture& subTexture, jv::ge::SubTexture* arr, const uint32_t length)
	{
		const float xPartition = (subTexture.rBot.x - subTexture.lTop.x) / static_cast<float>(length);
		const float yPartition = (subTexture.lTop.y - subTexture.rBot.y) / static_cast<float>(length);

		for (uint32_t i = 0; i < length; ++i)
			for (uint32_t j = 0; j < length; ++j)
			{
				auto& sub = arr[i * length + j] = subTexture;
				sub.lTop.x += xPartition * static_cast<float>(i);
				sub.rBot.x = sub.lTop.x + xPartition;
				sub.rBot.y += yPartition * static_cast<float>(j);
				sub.lTop.y = sub.rBot.y + yPartition;
			}
	}

	jv::ge::SubTexture Mirror(const jv::ge::SubTexture& subTexture)
	{
		auto ret = subTexture;
		ret.lTop.x = subTexture.rBot.x;
		ret.rBot.x = subTexture.lTop.x;
		return ret;
	}
}
