#include "pch_game.h"
#include "Engine/BasicRenderSystem.h"

#include "JLib/FileLoader.h"

namespace game
{
	BasicRenderSystem BasicRenderSystem::Create(jv::Arena& tempArena, const BasicRenderSystemCreateInfo& info)
	{
		BasicRenderSystem renderSystem{};

		const auto tempScope = tempArena.CreateScope();
		const auto vertCode = jv::file::Load(tempArena, info.vertPath);
		const auto fragCode = jv::file::Load(tempArena, info.fragPath);

		jv::ge::ShaderCreateInfo shaderCreateInfo{};
		shaderCreateInfo.vertexCode = vertCode.ptr;
		shaderCreateInfo.vertexCodeLength = vertCode.length;
		shaderCreateInfo.fragmentCode = fragCode.ptr;
		shaderCreateInfo.fragmentCodeLength = fragCode.length;
		renderSystem.shader = CreateShader(shaderCreateInfo);

		jv::ge::LayoutCreateInfo::Binding bindingCreateInfo{};
		bindingCreateInfo.stage = jv::ge::ShaderStage::fragment;
		bindingCreateInfo.type = jv::ge::BindingType::sampler;

		jv::ge::LayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.bindings = &bindingCreateInfo;
		layoutCreateInfo.bindingsCount = 1;

		renderSystem.layout = CreateLayout(layoutCreateInfo);

		jv::ge::RenderPassCreateInfo renderPassCreateInfo{};
		renderPassCreateInfo.target = info.drawsDirectlyToSwapChain ? 
			jv::ge::RenderPassCreateInfo::DrawTarget::swapChain : jv::ge::RenderPassCreateInfo::DrawTarget::image;
		renderSystem.renderPass = CreateRenderPass(renderPassCreateInfo);

		jv::ge::PipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.resolution = info.resolution;
		pipelineCreateInfo.shader = renderSystem.shader;
		pipelineCreateInfo.layoutCount = 1;
		pipelineCreateInfo.layouts = &renderSystem.layout;
		pipelineCreateInfo.renderPass = renderSystem.renderPass;
		renderSystem.pipeline = CreatePipeline(pipelineCreateInfo);

		jv::ge::BufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.size = info.storageBufferSize * info.frameCount;
		bufferCreateInfo.scene = info.scene;
		renderSystem.buffer = AddBuffer(bufferCreateInfo);

		tempArena.DestroyScope(tempScope);
		return renderSystem;
	}

	void BasicRenderSystem::Destroy(const BasicRenderSystem& renderSystem)
	{
	}
}
