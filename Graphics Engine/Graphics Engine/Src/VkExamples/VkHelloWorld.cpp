#include "pch.h"
#include "VkExamples/VkHelloWorld.h"

#include "JLib/ArrayUtils.h"
#include "JLib/FileLoader.h"
#include "Vk/VkImage.h"
#include "Vk/VkLayout.h"
#include "Vk/VkPipeline.h"
#include "Vk/VkShader.h"
#include "VkHL/VkMesh.h"
#include "VkHL/VkShapes.h"
#include "VkHL/VkTexture.h"

namespace jv::vk::example
{
	struct HelloWorldProgram final
	{
		uint64_t scope;
		Mesh mesh;
		Image image;
		Pipeline pipeline;
		VkShaderModule modules[2];
		VkDescriptorSetLayout layout;
	};

	void* OnBegin(Program& program)
	{
		const auto helloWorldProgram = program.arena.New<HelloWorldProgram>();
		helloWorldProgram->scope = program.arena.CreateScope();

		Array<Vertex2d> vertices;
		Array<VertexIndex> indices;
		const auto tempScope = CreateQuadShape(program.tempArena, vertices, indices);

		helloWorldProgram->mesh = Mesh::Create(program.vkCPUArena, program.vkGPUArena, program.vkApp, vertices, indices);

		constexpr ImageCreateInfo imageCreateInfo{};
		helloWorldProgram->image = LoadTexture(program.vkCPUArena, program.vkGPUArena, program.vkApp,
			imageCreateInfo, "Art/logo.png");

		const auto vertCode = file::Load(program.tempArena, "Shaders/vert.spv");
		const auto fragCode = file::Load(program.tempArena, "Shaders/frag.spv");

		const auto shaderModules = CreateArray<PipelineCreateInfo::Module>(program.tempArena, 2);
		shaderModules[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderModules[0].module = CreateShaderModule(program.vkApp, vertCode.ptr, vertCode.length);
		shaderModules[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderModules[1].module = CreateShaderModule(program.vkApp, fragCode.ptr, fragCode.length);

		for (uint32_t i = 0; i < 2; ++i)
			helloWorldProgram->modules[i] = shaderModules[i].module;

		const auto bindings = CreateArray<Binding>(program.tempArena, 1);
		bindings[0] = {};
		bindings[0].flag = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		helloWorldProgram->layout = CreateLayout(program.tempArena, program.vkApp, bindings);

		PipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.modules = shaderModules;
		pipelineCreateInfo.resolution = program.resolution;
		pipelineCreateInfo.renderPass = program.swapChainRenderPass;
		pipelineCreateInfo.getBindingDescriptions = Vertex2d::GetBindingDescriptions;
		pipelineCreateInfo.getAttributeDescriptions = Vertex2d::GetAttributeDescriptions;
		pipelineCreateInfo.layouts.ptr = &helloWorldProgram->layout;
		pipelineCreateInfo.layouts.length = 1;
		helloWorldProgram->pipeline = Pipeline::Create(pipelineCreateInfo, program.tempArena, program.vkApp);

		program.tempArena.DestroyScope(tempScope);
		return helloWorldProgram;
	}

	bool OnUpdate(Program& program, void* userPtr)
	{
		return true;
	}

	bool OnExit(Program& program, void* userPtr)
	{
		const auto helloWorldProgram = static_cast<HelloWorldProgram*>(userPtr);

		for (auto& mod : helloWorldProgram->modules)
			vkDestroyShaderModule(program.vkApp.device, mod, nullptr);
		vkDestroyDescriptorSetLayout(program.vkApp.device, helloWorldProgram->layout, nullptr);
		Pipeline::Destroy(helloWorldProgram->pipeline, program.vkApp);
		Image::Destroy(program.vkGPUArena, program.vkApp, helloWorldProgram->image);
		Mesh::Destroy(program.vkGPUArena, helloWorldProgram->mesh, program.vkApp);

		program.arena.DestroyScope(helloWorldProgram->scope);
		program.arena.Free(userPtr);

		return true;
	}

	bool OnRenderUpdate(Program& program, void* userPtr)
	{
		return true;
	}

	void DefineHelloWorldExample(ProgramInfo& info)
	{
		info.name = "Hello World";
		info.onBegin = OnBegin;
		info.onUpdate = OnUpdate;
		info.onExit = OnExit;
		info.onRenderUpdate = OnRenderUpdate;
	}
}
