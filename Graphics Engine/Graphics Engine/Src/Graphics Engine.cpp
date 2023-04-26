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

	jv::ge::ImageCreateInfo imageCreateInfo{};
	imageCreateInfo.resolution = { texWidth, texHeight };
	imageCreateInfo.scene = scene;
	const auto image = AddImage(imageCreateInfo);
	jv::ge::FillImage(image, pixels);

	stbi_image_free(pixels);

	jv::ge::BufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.size = 48;
	bufferCreateInfo.scene = scene2;
	const auto buffer = AddBuffer(bufferCreateInfo);

	uint64_t bufferData = 8;
	jv::ge::BufferUpdateInfo bufferUpdateInfo;
	bufferUpdateInfo.buffer = buffer;
	bufferUpdateInfo.data = &bufferData;
	bufferUpdateInfo.size = sizeof bufferData;
	UpdateBuffer(bufferUpdateInfo);

	jv::ge::Vertex2D vertices[4]
	{
		glm::vec2{ -1, -1 },
		glm::vec2{ -1, 1 },
		glm::vec2{ 1, 1 },
		glm::vec2{ 1, -1 }
	};
	uint16_t indices[6]{0, 1, 2, 0, 2, 3};

	jv::ge::MeshCreateInfo meshCreateInfo{};
	meshCreateInfo.verticesLength = 4;
	meshCreateInfo.indicesLength = 6;
	meshCreateInfo.vertices2d = vertices;
	meshCreateInfo.indices = indices;
	meshCreateInfo.scene = scene;
	const auto mesh = AddMesh(meshCreateInfo);

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

	jv::ge::LayoutCreateInfo::Binding bindingCreateInfo{};
	bindingCreateInfo.stage = jv::ge::ShaderStage::fragment;
	bindingCreateInfo.type = jv::ge::BindingType::sampler;

	jv::ge::LayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.bindings = &bindingCreateInfo;
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

	jv::ge::DescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.layout = layout;
	poolCreateInfo.capacity = 20;
	poolCreateInfo.scene = scene;
	const auto pool = AddDescriptorPool(poolCreateInfo);

	jv::ge::WriteInfo::Binding writeBindingInfo{};
	writeBindingInfo.type = jv::ge::BindingType::sampler;
	writeBindingInfo.image.image = image;
	writeBindingInfo.image.sampler = sampler;

	jv::ge::WriteInfo writeInfo{};
	writeInfo.pool = pool;
	writeInfo.descriptorIndex = 0;
	writeInfo.bindings = &writeBindingInfo;
	writeInfo.bindingCount = 1;
	Write(writeInfo);

	// todo render call, semaphores

	while (jv::ge::RenderFrame())
		;

	jv::ge::Shutdown();
}
