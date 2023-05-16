#include "pch.h"
#include "GE/GraphicsEngine.h"
#include <stb_image.h>

#include "JLib/ArrayUtils.h"
#include "JLib/FileLoader.h"
#include "RenderGraph/RenderGraph.h"

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
		jv::ge::RenderFrameInfo renderFrameInfo{};
		const auto result = RenderFrame(renderFrameInfo);
		assert(result);
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
		{glm::vec2{ -1, -1 }, glm::vec2{0, 0}},
		{glm::vec2{ -1, 1 },glm::vec2{0, 1}},
		{glm::vec2{ 1, 1 }, glm::vec2{1, 1}},
		{glm::vec2{ 1, -1 }, glm::vec2{1, 0}}
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

	jv::ge::RenderPassCreateInfo renderPassCreateInfo{};
	const auto renderPass = CreateRenderPass(renderPassCreateInfo);

	jv::ge::PipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.resolution = info.resolution;
	pipelineCreateInfo.shader = shader;
	pipelineCreateInfo.layoutCount = 1;
	pipelineCreateInfo.layouts = &layout;
	pipelineCreateInfo.renderPass = renderPass;
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
	writeInfo.descriptorSet = jv::ge::GetDescriptorSet(pool, 0);
	writeInfo.bindings = &writeBindingInfo;
	writeInfo.bindingCount = 1;
	Write(writeInfo);

	{
		const auto mem = malloc(4096);

		jv::ArenaCreateInfo arenaInfo{};
		arenaInfo.alloc = Alloc;
		arenaInfo.free = Free;
		arenaInfo.memory = mem;
		arenaInfo.memorySize = 4096;
		auto tempArena = jv::Arena::Create(arenaInfo);
		const auto tempScope = tempArena.CreateScope();

		const auto resources = jv::CreateArray<ge::ResourceMaskDescription>(tempArena, 11);
		const auto nodes = jv::CreateArray<ge::RenderGraphNodeInfo>(tempArena, 10);

		uint32_t n0OutResource = 0;
		nodes[0].inResourceCount = 0;
		nodes[0].outResourceCount = 1;
		nodes[0].outResources = &n0OutResource;

		uint32_t n1OutResources[2];
		n1OutResources[0] = 1;
		n1OutResources[1] = 2;
		nodes[1].inResourceCount = 0;
		nodes[1].outResourceCount = 2;
		nodes[1].outResources = n1OutResources;

		uint32_t n2InResources[2];
		uint32_t n2OutResource = 3;
		n2InResources[0] = 2;
		n2InResources[1] = 4;
		nodes[2].inResourceCount = 2;
		nodes[2].inResources = n2InResources;
		nodes[2].outResourceCount = 1;
		nodes[2].outResources = &n2OutResource;

		uint32_t n3OutResource = 4;
		nodes[3].inResourceCount = 0;
		nodes[3].outResourceCount = 1;
		nodes[3].outResources = &n3OutResource;

		uint32_t n4InResources[2];
		uint32_t n4OutResource = 5;
		n4InResources[0] = n0OutResource;
		n4InResources[1] = n1OutResources[0];
		nodes[4].inResourceCount = 2;
		nodes[4].inResources = n4InResources;
		nodes[4].outResourceCount = 1;
		nodes[4].outResources = &n4OutResource;

		uint32_t n5InResources[2];
		uint32_t n5OutResource = 6;
		n5InResources[0] = n4OutResource;
		n5InResources[1] = n1OutResources[0];
		nodes[5].inResourceCount = 2;
		nodes[5].inResources = n5InResources;
		nodes[5].outResourceCount = 1;
		nodes[5].outResources = &n5OutResource;

		uint32_t n6InResources[3];
		uint32_t n6OutResource = 7;
		n6InResources[0] = n1OutResources[0];
		n6InResources[1] = n1OutResources[1];
		n6InResources[2] = n2OutResource;
		nodes[6].inResourceCount = 3;
		nodes[6].inResources = n6InResources;
		nodes[6].outResourceCount = 1;
		nodes[6].outResources = &n6OutResource;

		uint32_t n8OutResource = 8;
		uint32_t n9OutResource = 9;

		uint32_t n7InResources[4];
		n7InResources[0] = n5OutResource;
		n7InResources[1] = n6OutResource;
		n7InResources[2] = n8OutResource;
		n7InResources[3] = n9OutResource;
		nodes[7].inResourceCount = 4;
		nodes[7].inResources = n7InResources;
		nodes[7].outResourceCount = 0;

		nodes[8].inResourceCount = 0;
		nodes[8].outResourceCount = 1;
		nodes[8].outResources = &n8OutResource;

		nodes[9].inResourceCount = 0;
		nodes[9].outResourceCount = 1;
		nodes[9].outResources = &n9OutResource;

		ge::RenderGraphCreateInfo renderGraphCreateInfo{};
		renderGraphCreateInfo.resources = resources.ptr;
		renderGraphCreateInfo.resourceCount = resources.length;
		renderGraphCreateInfo.nodes = nodes.ptr;
		renderGraphCreateInfo.nodeCount = nodes.length;
		const auto graph = ge::RenderGraph::Create(arena, tempArena, renderGraphCreateInfo);

		ge::RenderGraph::Destroy(arena, graph);
		free(mem);
		tempArena.DestroyScope(tempScope);
	}

	bool quit = false;
	while (!quit)
	{
		jv::ge::DrawInfo drawInfo{};
		drawInfo.pipeline = pipeline;
		drawInfo.mesh = mesh;
		drawInfo.descriptorSetCount = 1;
		drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(pool, 0);
		drawInfo.instanceCount = 1;
		Draw(drawInfo);

		jv::ge::RenderFrameInfo renderFrameInfo{};
		quit = !RenderFrame(renderFrameInfo);
	}

	jv::ge::Shutdown();
}
