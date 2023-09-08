#pragma once
#include "Engine/Engine.h"
#include "Engine/TaskSystem.h"
#include "GE/GraphicsEngine.h"
#include <stb_image.h>
#include "JLib/FileLoader.h"

namespace game
{
	struct InstancedRenderInterpreterCreateInfo final
	{
		glm::ivec2 resolution;

		const char* fragPath = "Shaders/frag.spv";
		const char* vertPath = "Shaders/vert.spv";
		bool drawsDirectlyToSwapChain = true;
	};

	struct InstancedRenderInterpreterEnableInfo final
	{
		jv::ge::Resource scene;
		uint32_t capacity;
	};

	template <typename Task>
	class InstancedRenderInterpreter final : public TaskInterpreter<Task, InstancedRenderInterpreterCreateInfo>
	{
	public:
		struct Camera
		{
			glm::vec2 position{};
			float zoom = 0;
			float rotation = 0;
		} camera{};

		jv::ge::Resource image = nullptr;
		jv::ge::Resource mesh = nullptr;
		
		void Enable(const InstancedRenderInterpreterEnableInfo& info)
		{
			_capacity = info.capacity;

			jv::ge::BufferCreateInfo bufferCreateInfo{};
			bufferCreateInfo.size = info.capacity * jv::ge::GetFrameCount() * static_cast<uint32_t>(sizeof(Task));
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
				writeBindingInfo.buffer.offset = sizeof(Task) * info.capacity * i;
				writeBindingInfo.buffer.range = sizeof(Task) * info.capacity;
				writeBindingInfo.index = 0;

				jv::ge::WriteInfo writeInfo{};
				writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, i);
				writeInfo.bindings = &writeBindingInfo;
				writeInfo.bindingCount = 1;
				Write(writeInfo);
			}
		}

	private:
		struct PushConstant final
		{
			Camera camera{};
			glm::vec2 resolution;
		} _pushConstant{};
		
		InstancedRenderInterpreterCreateInfo _createInfo;
		uint32_t _capacity;

		jv::ge::Resource _shader;
		jv::ge::Resource _layout;
		jv::ge::Resource _renderPass;
		jv::ge::Resource _pipeline;

		jv::ge::Resource _buffer;
		jv::ge::Resource _fallbackImage;
		jv::ge::Resource _fallbackMesh;
		jv::ge::Resource _sampler;
		jv::ge::Resource _pool;

		void OnStart(const InstancedRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory) override
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
			bindingCreateInfos[0].stage = jv::ge::ShaderStage::vertex;
			bindingCreateInfos[0].type = jv::ge::BindingType::storageBuffer;
			bindingCreateInfos[1].stage = jv::ge::ShaderStage::fragment;
			bindingCreateInfos[1].type = jv::ge::BindingType::sampler;

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
			pipelineCreateInfo.vertexType = jv::ge::VertexType::v3D;
			_pipeline = CreatePipeline(pipelineCreateInfo);

			memory.tempArena.DestroyScope(tempScope);
		}

		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<Task>>& tasks) override
		{
			const auto& renderedTasks = tasks[0];
			if (renderedTasks.count == 0)
				return;

			const uint32_t frameIndex = jv::ge::GetFrameIndex();

			jv::ge::BufferUpdateInfo bufferUpdateInfo{};
			bufferUpdateInfo.buffer = _buffer;
			bufferUpdateInfo.size = sizeof(Task) * _capacity;
			bufferUpdateInfo.offset = sizeof(Task) * _capacity * frameIndex;
			bufferUpdateInfo.data = renderedTasks.ptr;
			UpdateBuffer(bufferUpdateInfo);

			jv::ge::WriteInfo::Binding writeBindingInfo{};
			writeBindingInfo.type = jv::ge::BindingType::sampler;
			writeBindingInfo.image.image = image ? image : _fallbackImage;
			writeBindingInfo.image.sampler = _sampler;
			writeBindingInfo.index = 1;

			jv::ge::WriteInfo writeInfo{};
			writeInfo.descriptorSet = jv::ge::GetDescriptorSet(_pool, frameIndex);
			writeInfo.bindings = &writeBindingInfo;
			writeInfo.bindingCount = 1;
			Write(writeInfo);

			_pushConstant.camera = camera;
			_pushConstant.resolution = _createInfo.resolution;

			jv::ge::DrawInfo drawInfo{};
			drawInfo.pipeline = _pipeline;
			drawInfo.mesh = mesh ? mesh : _fallbackMesh;
			drawInfo.descriptorSets[0] = jv::ge::GetDescriptorSet(_pool, frameIndex);
			drawInfo.descriptorSetCount = 1;
			drawInfo.instanceCount = renderedTasks.count;
			drawInfo.pushConstant = &_pushConstant;
			drawInfo.pushConstantSize = sizeof(PushConstant);
			Draw(drawInfo);
		}

		void OnExit(const EngineMemory& memory) override
		{
		}
	};
}
