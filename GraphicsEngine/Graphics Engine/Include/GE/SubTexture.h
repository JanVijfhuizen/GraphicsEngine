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
	};
}