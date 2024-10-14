#pragma once

namespace jv::ge
{
	struct SubTexture final
	{
		glm::vec2 lTop{ 0 };
		glm::vec2 rBot{ 1 };

		[[nodiscard]] SubTexture MirrorHorizontal() const
		{
			SubTexture ret = *this;
			ret.lTop.x = rBot.x;
			ret.rBot.x = lTop.x;
			return ret;
		}

		[[nodiscard]] glm::vec2 Center() const 
		{
			return { lTop.x + (rBot.x - lTop.x) / 2, rBot.y + (lTop.y - rBot.y) / 2 };
		}

		[[nodiscard]] glm::vec2 Size() const
		{
			return glm::vec2(rBot.x - lTop.x, lTop.y - rBot.y);
		}
	};
}