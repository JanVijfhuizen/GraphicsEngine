#include "pch_game.h"
#include "Engine/InstancedRenderInterpreter.h"
#include <stb_image.h>
#include "JLib/FileLoader.h"

namespace game
{
	void InstancedRenderInterpreter::Enable(const InstancedRenderInterpreterEnableInfo& info)
	{
		assert(!_enabled);
		_enabled = true;

		jv::ge::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = info.capacity * jv::ge::GetFrameCount() * static_cast<uint32_t>(sizeof(InstancedRenderTask));
		bufferCreateInfo.scene = info.scene;
		bufferCreateInfo.type = jv::ge::BufferCreateInfo::Type::storage;
		_buffer = AddBuffer(bufferCreateInfo);

		int texWidth, texHeight, texChannels2;
		stbi_uc* pixels = stbi_load("Art/fallback.png", &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);

		jv::ge::ImageCreateInfo imageCreateInfo{};
		imageCreateInfo.resolution = { texWidth, texHeight };
		imageCreateInfo.scene = info.scene;
		_fallbackImage = AddImage(imageCreateInfo);
		jv::ge::FillImage(_fallbackImage, pixels);

		jv::ge::Vertex2D vertices[4]
		{
			{glm::vec2{ -1, -1 }, glm::vec2{0, 0}},
			{glm::vec2{ -1, 1 },glm::vec2{0, 1}},
			{glm::vec2{ 1, 1 }, glm::vec2{1, 1}},
			{glm::vec2{ 1, -1 }, glm::vec2{1, 0}}
		};
		uint16_t indices[6]{ 0, 1, 2, 0, 2, 3 };

		jv::ge::MeshCreateInfo meshCreateInfo{};
		meshCreateInfo.verticesLength = 4;
		meshCreateInfo.indicesLength = 6;
		meshCreateInfo.vertices = vertices;
		meshCreateInfo.indices = indices;
		meshCreateInfo.scene = info.scene;
		_fallbackMesh = AddMesh(meshCreateInfo);

		jv::ge::SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.scene = info.scene;
		_sampler = AddSampler(samplerCreateInfo);

		const uint32_t frameCount = jv::ge::GetFrameCount();

		jv::ge::DescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.layout = _layout;
		poolCreateInfo.capacity = frameCount;
		poolCreateInfo.scene = info.scene;
		_pool = AddDescriptorPool(poolCreateInfo);

		for (uint32_t i = 0; i < frameCount; ++i)
		{
			jv::ge::WriteInfo::Binding writeBindingInfo{};
			writeBindingInfo.type = jv::ge::BindingType::storageBuffer;
			writeBindingInfo.buffer.buffer = _buffer;
			writeBindingInfo.buffer.offset = sizeof(InstancedRenderTask) * info.capacity * i;
			writeBindingInfo.buffer.range = sizeof(InstancedRenderTask) * info.capacity;
			writeBindingInfo.index = 1;

			jv::ge::WriteInfo writeInfo{};
			writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, i);
			writeInfo.bindings = &writeBindingInfo;
			writeInfo.bindingCount = 1;
			Write(writeInfo);
		}
	}

	void InstancedRenderInterpreter::Disable()
	{
		assert(_enabled);
		_enabled = false;
	}

	void InstancedRenderInterpreter::OnStart(const InstancedRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory)
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

		jv::ge::LayoutCreateInfo::Binding bindingCreateInfos[2]{};
		bindingCreateInfos[0].stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfos[0].type = jv::ge::BindingType::sampler;
		bindingCreateInfos[1].stage = jv::ge::ShaderStage::vertex;
		bindingCreateInfos[1].type = jv::ge::BindingType::storageBuffer;

		jv::ge::LayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.bindings = bindingCreateInfos;
		layoutCreateInfo.bindingsCount = 2;

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
		_pipeline = CreatePipeline(pipelineCreateInfo);

		memory.tempArena.DestroyScope(tempScope);
	}

	void InstancedRenderInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<InstancedRenderTask>>& tasks)
	{
		const auto& renderedTasks = tasks[0];
		if (renderedTasks.count == 0)
			return;

		const uint32_t frameIndex = jv::ge::GetFrameIndex();
		
		jv::ge::WriteInfo::Binding writeBindingInfo{};
		writeBindingInfo.type = jv::ge::BindingType::sampler;
		writeBindingInfo.image.image = image ? image : _fallbackImage;
		writeBindingInfo.image.sampler = _sampler;

		jv::ge::WriteInfo writeInfo{};
		writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, frameIndex);
		writeInfo.bindings = &writeBindingInfo;
		writeInfo.bindingCount = 1;
		Write(writeInfo);

		jv::ge::DrawInfo drawInfo{};
		drawInfo.pipeline = _pipeline;
		drawInfo.mesh = mesh ? mesh : _fallbackMesh;
		drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(_pool, frameIndex);
		drawInfo.descriptorSetCount = 1;
		drawInfo.instanceCount = renderedTasks.count;
		drawInfo.pushConstant = &pushConstant;
		drawInfo.pushConstantSize = sizeof(PushConstant);
		drawInfo.pushConstantStage = jv::ge::ShaderStage::vertex;
		Draw(drawInfo);
	}

	void InstancedRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
