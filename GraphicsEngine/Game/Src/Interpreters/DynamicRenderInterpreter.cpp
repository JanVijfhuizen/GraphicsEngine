#include "pch_game.h"
#include "Interpreters/DynamicRenderInterpreter.h"
#include <stb_image.h>

#include "GE/GraphicsEngine.h"
#include "JLib/FileLoader.h"
#include "JLib/Math.h"

namespace game
{
	constexpr uint32_t LIGHT_CAPACITY = 16;

	struct LightInfo final
	{
		glm::vec3 ambient{.1f};
		uint32_t count;
	};

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

		pixels = stbi_load("Art/fallback_normal.png", &texWidth, &texHeight, &texChannels2, STBI_rgb_alpha);
		_fallbackNormalImage = AddImage(imageCreateInfo);
		jv::ge::FillImage(_fallbackNormalImage, pixels);
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
		const uint32_t resourceCount = frameCount * info.capacity;

		_samplers = jv::CreateArray<jv::ge::Resource>(*info.arena, resourceCount * 2);
		jv::ge::SamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.scene = info.scene;
		for (auto& sampler : _samplers)
			sampler = AddSampler(samplerCreateInfo);

		jv::ge::DescriptorPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.layout = _layout;
		poolCreateInfo.capacity = resourceCount;
		poolCreateInfo.scene = info.scene;
		_pool = AddDescriptorPool(poolCreateInfo);
		_frameCapacity = info.capacity;

		_lightInfoSize = jv::ge::GetMinUniformOffset(sizeof(LightInfo));
		_lightBufferSize = jv::ge::GetMinUniformOffset(sizeof(LightTask) * LIGHT_CAPACITY);

		jv::ge::BufferCreateInfo lightInfoBufferCreateInfo{};
		lightInfoBufferCreateInfo.size = _lightInfoSize * jv::ge::GetFrameCount();
		lightInfoBufferCreateInfo.scene = info.scene;
		lightInfoBufferCreateInfo.type = jv::ge::BufferCreateInfo::Type::uniform;
		_lightInfoBuffer = AddBuffer(lightInfoBufferCreateInfo);

		jv::ge::BufferCreateInfo lightsBufferCreateInfo{};
		lightsBufferCreateInfo.size = _lightBufferSize * jv::ge::GetFrameCount();
		lightsBufferCreateInfo.scene = info.scene;
		lightsBufferCreateInfo.type = jv::ge::BufferCreateInfo::Type::storage;
		_lightsBuffer = AddBuffer(lightsBufferCreateInfo);

		for (uint32_t i = 0; i < frameCount; ++i)
		{
			jv::ge::WriteInfo::Binding writeInfos[2]{};

			auto& uniformWriteBindingInfo = writeInfos[0];
			uniformWriteBindingInfo.type = jv::ge::BindingType::uniformBuffer;
			uniformWriteBindingInfo.buffer.buffer = _lightInfoBuffer;
			uniformWriteBindingInfo.buffer.offset = _lightInfoSize * i;
			uniformWriteBindingInfo.buffer.range = _lightInfoSize;
			uniformWriteBindingInfo.index = 2;

			auto& storageWriteBindingInfo = writeInfos[1];
			storageWriteBindingInfo.type = jv::ge::BindingType::storageBuffer;
			storageWriteBindingInfo.buffer.buffer = _lightsBuffer;
			storageWriteBindingInfo.buffer.offset = _lightBufferSize * i;
			storageWriteBindingInfo.buffer.range = _lightBufferSize;
			storageWriteBindingInfo.index = 3;

			jv::ge::WriteInfo writeInfo{};
			writeInfo.bindings = writeInfos;
			writeInfo.bindingCount = 2;

			for (uint32_t j = 0; j < _capacity; ++j)
			{
				writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, i * _capacity + j);
				Write(writeInfo);
			}
		}
	}

	jv::ge::Resource DynamicRenderInterpreter::GetFallbackMesh() const
	{
		return _fallbackMesh;
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

		jv::ge::LayoutCreateInfo::Binding bindingCreateInfos[4]{};
		bindingCreateInfos[0].stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfos[0].type = jv::ge::BindingType::sampler;
		bindingCreateInfos[1].stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfos[1].type = jv::ge::BindingType::sampler;
		bindingCreateInfos[2].stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfos[2].type = jv::ge::BindingType::uniformBuffer;
		bindingCreateInfos[3].stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfos[3].type = jv::ge::BindingType::storageBuffer;

		jv::ge::LayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.bindings = bindingCreateInfos;
		layoutCreateInfo.bindingsCount = 4;

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

		// Update lighting.
		{
			const auto& lightTasks = _createInfo.lightTasks->GetTaskBatches()[0];
			LightInfo lightInfo{};
			lightInfo.count = lightTasks.count;
			lightInfo.ambient = glm::vec3(1);

			jv::ge::BufferUpdateInfo bufferUpdateInfo{};
			bufferUpdateInfo.buffer = _lightInfoBuffer;
			bufferUpdateInfo.size = _lightInfoSize;
			bufferUpdateInfo.offset = _lightInfoSize * frameIndex;
			bufferUpdateInfo.data = &lightInfo; 
			UpdateBuffer(bufferUpdateInfo);

			if(lightTasks.count > 0)
			{
				jv::ge::BufferUpdateInfo storageBufferUpdateInfo{};
				storageBufferUpdateInfo.buffer = _lightsBuffer;
				storageBufferUpdateInfo.size = _lightBufferSize;
				storageBufferUpdateInfo.offset = _lightBufferSize * frameIndex;
				storageBufferUpdateInfo.data = lightTasks.ptr;
				UpdateBuffer(storageBufferUpdateInfo);
			}
		}

		for (const auto& batch : tasks)
			for (const auto& task : batch)
			{
				jv::ge::WriteInfo::Binding bindings[2]{};

				// Diffuse.
				auto& diffWriteBindingInfo = bindings[0];
				diffWriteBindingInfo.type = jv::ge::BindingType::sampler;
				diffWriteBindingInfo.image.image = task.image ? task.image : _fallbackImage;
				diffWriteBindingInfo.image.sampler = _samplers[i];
				diffWriteBindingInfo.index = 0;

				// Normal map.
				auto& normWriteBindingInfo = bindings[1];
				normWriteBindingInfo.type = jv::ge::BindingType::sampler;
				normWriteBindingInfo.image.image = task.normalImage ? task.normalImage : _fallbackNormalImage;
				normWriteBindingInfo.image.sampler = _samplers[i + _samplers.length / 2];
				normWriteBindingInfo.index = 1;

				jv::ge::WriteInfo writeInfo{};
				writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, i);
				writeInfo.bindings = bindings;
				writeInfo.bindingCount = 2;
				Write(writeInfo);

				PushConstant& pushConstant = *_createInfo.frameArena->New<PushConstant>();
				pushConstant.renderTask = task.renderTask;
				pushConstant.camera = camera;
				pushConstant.resolution = _createInfo.resolution;

				jv::ge::DrawInfo drawInfo{};
				drawInfo.pipeline = _pipeline;
				drawInfo.mesh = task.mesh ? task.mesh : _fallbackMesh;
				drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(_pool, i);
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
