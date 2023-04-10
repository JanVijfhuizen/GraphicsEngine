#include "pch.h"

#include "GE/GraphicsEngine.h"

#include <stb_image.h>

int main()
{
	jv::ge::CreateInfo info{};
	Initialize(info);

	for (int i = 0; i < 100; ++i)
	{
		jv::ge::RenderFrame();
	}

	const auto scene = jv::ge::CreateScene();
	const auto scene2 = jv::ge::CreateScene();
	
	int texWidth, texHeight, texChannels2;
	stbi_uc* pixels = stbi_load("Art/logo.png", &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

	jv::ge::ImageCreateInfo ici{};
	ici.resolution = { texWidth, texHeight };
	const auto image = AddImage(ici, scene);
	jv::ge::FillImage(scene, image, pixels);

	stbi_image_free(pixels);

	jv::ge::BufferCreateInfo bci{};
	bci.size = 48;
	const auto buffer = AddBuffer(bci, scene2);

	uint64_t ui = 8;
	jv::ge::UpdateBuffer(scene2, buffer, &ui, sizeof ui, 0);

	jv::ge::Vertex2D vertices[4]
	{
		glm::vec2{ -1, -1 },
		glm::vec2{ -1, 1 },
		glm::vec2{ 1, 1 },
		glm::vec2{ 1, -1 }
	};
	uint16_t indices[6]{0, 1, 2, 0, 2, 3};

	jv::ge::MeshCreateInfo mci{};
	mci.verticesLength = 4;
	mci.indicesLength = 6;
	mci.vertices2d = vertices;
	mci.indices = indices;
	const auto mesh = AddMesh(mci, scene);

	jv::ge::Resize(glm::ivec2(800), false);

	while (jv::ge::RenderFrame())
		;

	jv::ge::Shutdown();
}
