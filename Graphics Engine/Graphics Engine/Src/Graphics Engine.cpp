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
	ici.scene = scene;
	const auto image = AddImage(ici);
	jv::ge::FillImage(image, pixels);

	stbi_image_free(pixels);

	jv::ge::BufferCreateInfo bci{};
	bci.size = 48;
	bci.scene = scene2;
	const auto buffer = AddBuffer(bci);

	uint64_t ui = 8;
	jv::ge::BufferUpdateInfo bui;
	bui.buffer = buffer;
	bui.data = &ui;
	bui.size = sizeof ui;
	UpdateBuffer(bui);

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
	mci.scene = scene;
	const auto mesh = AddMesh(mci);

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

	jv::ge::LayoutCreateInfo::Binding binding{};
	binding.stage = jv::ge::ShaderStage::fragment;
	binding.type = jv::ge::BindingType::sampler;

	jv::ge::LayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.bindings = &binding;
	layoutCreateInfo.bindingsCount = 1;

	auto layout = CreateLayout(layoutCreateInfo);
	
	jv::ge::PipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.resolution = info.resolution;
	pipelineCreateInfo.shader = shader;
	pipelineCreateInfo.layoutCount = 1;
	pipelineCreateInfo.layouts = &layout;
	const auto pipeline = CreatePipeline(pipelineCreateInfo);

	jv::ge::SamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.scene = scene;
	const auto sampler = AddSampler(samplerCreateInfo);

	jv::ge::PoolCreateInfo poolCreateInfo{};
	poolCreateInfo.layout = layout;
	poolCreateInfo.capacity = 20;
	poolCreateInfo.scene = scene;
	const auto pool = AddPool(poolCreateInfo);

	jv::ge::BindInfo::Write write{};
	write.type = jv::ge::BindingType::sampler;
	write.image.image = image;
	write.image.sampler = sampler;

	jv::ge::BindInfo bindInfo{};
	bindInfo.pool = pool;
	bindInfo.descriptorIndex = 0;
	bindInfo.writes = &write;
	bindInfo.writeCount = 1;
	Bind(bindInfo);

	// todo render call, semaphores

	while (jv::ge::RenderFrame())
		;

	jv::ge::Shutdown();
}
