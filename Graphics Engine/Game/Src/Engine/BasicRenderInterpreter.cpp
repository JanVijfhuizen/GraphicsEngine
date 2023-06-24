#include "pch_game.h"
#include "Engine/BasicRenderInterpreter.h"
#include "JLib/FileLoader.h"

namespace game
{
	void BasicRenderInterpreter::OnStart(const BasicRenderInterpreterCreateInfo& createInfo, const EngineMemory& memory)
	{
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
		_pipeline = CreatePipeline(pipelineCreateInfo);

		jv::ge::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = createInfo.capacity * createInfo.frameCount * static_cast<uint32_t>(sizeof(RenderTask));
		bufferCreateInfo.scene = createInfo.scene;
		_buffer = AddBuffer(bufferCreateInfo);

		memory.tempArena.DestroyScope(tempScope);
	}

	void BasicRenderInterpreter::OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<RenderTask>>& tasks)
	{
	}

	void BasicRenderInterpreter::OnExit(const EngineMemory& memory)
	{
	}
}
