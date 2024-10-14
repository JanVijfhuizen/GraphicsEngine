#pragma once

namespace jv
{
	namespace ge
	{
		struct SubTexture;
	}
}

namespace game
{
	void Divide(const jv::ge::SubTexture& subTexture, jv::ge::SubTexture* arr, uint32_t length);
	void Divide2d(const jv::ge::SubTexture& subTexture, jv::ge::SubTexture* arr, uint32_t length);
	jv::ge::SubTexture Mirror(const jv::ge::SubTexture& subTexture);
}
