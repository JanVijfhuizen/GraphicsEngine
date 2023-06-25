#include "pch_game.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include <stb_image.h>
#include "JLib/FileLoader.h"

namespace game
{
	void DynamicRenderInterpreter::Enable(const DynamicRenderInterpreterEnableInfo& info)
	{
		_capacity = info.capacity;

		int texWidth, texHeight, texChannels2;
		stbi_uc* pixels = stbi_load("Art/fallback.png", &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

		jv::ge::ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.resolution = { texWidth, texHeight };
		imageCreateInfo.scene = info.scene;
		_fallbackImage = AddImage(imageCreateInfo);
		jv::ge::FillImage(_fallbackImage, pixels);
		stbi_image_free(pixels);

		jv::ge::Vertex3D vertices[4]
		{
			{glm::vec3{ -1, -1, 0 }, glm::vec3{0, 0, 1}, glm::vec2{0, 0}},
			{glm::vec3{ -1, 1, 0 },glm::vec3{0, 0, 1}, glm::vec2{0, 1}},
			{glm::vec3{ 1, 1, 0 }, glm::vec3{0, 0, 1}, glm::vec2{1, 1}},
			{glm::vec3{ 1, -1, 0 }, glm::vec3{0, 0, 1}, glm::vec2{1, 0}}
		};
		uint16_t indices[6]{ 0, 1, 2, 0, 2, 3 };

		jv::ge::MeshCreateInfo meshCreateInfo{};
		meshCreateInfo.verticesLength = 4;
		meshCreateInfo.indicesLength = 6;
		meshCreateInfo.vertices = vertices;
		meshCreateInfo.indices = indices;
		meshCreateInfo.vertexType = jv::ge::VertexType::v3D;
		meshCreateInfo.scene = info.scene;
		_fallbackMesh = AddMesh(meshCreateInfo);

		const uint32_t frameCount = jv::ge::GetFrameCount();
		const uint32_t descriptorSetCount = frameCount * info.capacity;

		_samplers = jv::CreateArray<jv::ge::Resource>(*info.arena, descriptorSetCount);
		jv::ge::SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.scene = info.scene;
		for (auto& sampler : _samplers)
			sampler = AddSampler(samplerCreateInfo);

		jv::ge::DescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.layout = _layout;
		poolCreateInfo.capacity = descriptorSetCount;
		poolCreateInfo.scene = info.scene;
		_pool = AddDescriptorPool(poolCreateInfo);
	}

	void DynamicRenderInterpreter::OnStart(const DynamicRenderInterpreterCreateInfo& createInfo,
		const EngineMemory& memory)
	{
		_createInfo = createInfo;

		const auto tempScope = memory.tempArena.CreateScope();
		const auto vertCode = jv::file::Load(memory.tempArena, createInfo.vertPath);
		const auto fragCode = jv::file::Load(memory.tempArena, createInfo.fragPath);

		jv::ge::ShaderCreateInfo shaderCreateInfo{};
		shaderCreateInfo.vertexCode = vertCode.ptr;
		shaderCreateInfo.vertexCodeLength = vertCode.length;
		shaderCreateInfo.fragmentCode = fragCode.ptr;
		shaderCreateInfo.fragmentCodeLength = fragCode.length;
		_shader = CreateShader(shaderCreateInfo);

		jv::ge::LayoutCreateInfo::Binding bindingCreateInfo{};
		bindingCreateInfo.stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfo.type = jv::ge::BindingType::sampler;

		jv::ge::LayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.bindings = &bindingCreateInfo;
		layoutCreateInfo.bindingsCount = 1;

		_layout = CreateLayout(layoutCreateInfo);

		jv::ge::RenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.target = createInfo.drawsDirectlyToSwapChain ?
			jv::ge::RenderPassCreateInfo::DrawTarget::swapChain : jv::ge::RenderPassCreateInfo::DrawTarget::image;
		_renderPass = CreateRenderPass(renderPassCreateInfo);

		jv::ge::PipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.resolution = createInfo.resolution;
		pipelineCreateInfo.shader = _shader;
		pipelineCreateInfo.layoutCount = 1;
		pipelineCreateInfo.layouts = &_layout;
		pipelineCreateInfo.renderPass = _renderPass;
		pipelineCreateInfo.pushConstantSize = sizeof(PushConstant);
		pipelineCreateInfo.vertexType = jv::ge::VertexType::v3D;
		_pipeline = CreatePipeline(pipelineCreateInfo);

		memory.tempArena.DestroyScope(tempScope);
	}

	void DynamicRenderInterpreter::OnUpdate(const EngineMemory& memory,
		const jv::LinkedList<jv::Vector<DynamicRenderTask>>& tasks)
	{
		const uint32_t frameIndex = jv::ge::GetFrameIndex();
		const uint32_t startIndex = _capacity * frameIndex;

		uint32_t i = startIndex;

		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				jv::ge::WriteInfo::Binding writeBindingInfo{};
				writeBindingInfo.type = jv::ge::BindingType::sampler;
				writeBindingInfo.image.image = task.image ? task.image : _fallbackImage;
				writeBindingInfo.image.sampler = _samplers[i];
				writeBindingInfo.index = 0;

				jv::ge::WriteInfo writeInfo{};
				writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, frameIndex);
				writeInfo.bindings = &writeBindingInfo;
				writeInfo.bindingCount = 1;
				Write(writeInfo);

				PushConstant& pushConstant = *_createInfo.frameArena->New<PushConstant>();
				pushConstant.renderTask = task.renderTask;
				pushConstant.camera = camera;
				pushConstant.resolution = _createInfo.resolution;

				jv::ge::DrawInfo drawInfo{};
				drawInfo.pipeline = _pipeline;
				drawInfo.mesh = task.mesh ? task.mesh : _fallbackMesh;
				drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(_pool, frameIndex);
				drawInfo.descriptorSetCount = 1;
				drawInfo.pushConstant = &pushConstant;
				drawInfo.pushConstantSize = sizeof(PushConstant);
				Draw(drawInfo);

				++i;
			}
	}

	void DynamicRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
