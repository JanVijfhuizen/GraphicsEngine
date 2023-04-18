#include "pch.h"

#include "GE/GraphicsEngine.h"

#include <stb_image.h>

#include "JLib/FileLoader.h"

void* Alloc(const uint32_t size)
{
	return malloc(size);
}

void Free(void* ptr)
{
	return free(ptr);
}

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

	jv::ArenaCreateInfo arenaCreateInfo{};
	arenaCreateInfo.memory = malloc(4096);
	arenaCreateInfo.memorySize = 4096;
	arenaCreateInfo.alloc = Alloc;
	arenaCreateInfo.free = Free;
	auto arena = jv::Arena::Create(arenaCreateInfo);
	const auto vertCode = jv::file::Load(arena, "Shaders/vert.spv");
	const auto fragCode = jv::file::Load(arena, "Shaders/frag.spv");

	jv::ge::ShaderCreateInfo shaderCreateInfo{};
	shaderCreateInfo.vertexCode = vertCode.ptr;
	shaderCreateInfo.vertexCodeLength = vertCode.length;
	shaderCreateInfo.fragmentCode = fragCode.ptr;
	shaderCreateInfo.fragmentCodeLength = fragCode.length;
	const auto shader = CreateShader(shaderCreateInfo);

	jv::ge::PipelineCreateInfo::Binding binding{};
	binding.stage = jv::ge::ShaderStage::fragment;
	binding.type = jv::ge::BindingType::sampler;

	jv::ge::PipelineCreateInfo::Layout layout{};
	layout.bindingsCount = 1;
	layout.bindings = &binding;

	jv::ge::PipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.resolution = info.resolution;
	pipelineCreateInfo.shader = shader;
	pipelineCreateInfo.layoutCount = 1;
	pipelineCreateInfo.layouts = &layout;
	const auto pipeline = CreatePipeline(pipelineCreateInfo);

	while (jv::ge::RenderFrame())
		;

	jv::ge::Shutdown();
}
