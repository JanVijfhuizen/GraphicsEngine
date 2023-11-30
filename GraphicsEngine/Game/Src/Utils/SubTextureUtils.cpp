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

	jv::ge::SubTexture Mirror(const jv::ge::SubTexture& subTexture)
	{
		auto ret = subTexture;
		ret.lTop.x = subTexture.rBot.x;
		ret.rBot.x = subTexture.lTop.x;
		return ret;
	}
}
