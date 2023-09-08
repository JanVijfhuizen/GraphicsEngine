#pragma once
#include "RenderTask.h"

namespace game
{
	struct DynamicRenderTask final
	{
		jv::ge::Resource image = nullptr;
		jv::ge::Resource mesh = nullptr;
		RenderTask renderTask{};
	};
}
