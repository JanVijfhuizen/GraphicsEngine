#include "pch.h"

#include "GE/GraphicsEngine.h"

int main()
{
	jv::ge::CreateInfo info{};
	Initialize(info);

	for (int i = 0; i < 100; ++i)
	{
		jv::ge::RenderFrame();
	}

	jv::ge::Resize(glm::ivec2(800), false);

	while (jv::ge::RenderFrame())
		;

	jv::ge::Shutdown();
}
