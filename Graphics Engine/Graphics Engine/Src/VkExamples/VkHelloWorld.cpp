#include "pch.h"
#include "VkExamples/VkHelloWorld.h"

#include "JLib/FileLoader.h"
#include "Vk/VkImage.h"
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

		//file::Load(program.tempArena, "Shaders/helloWorldFrag.");

		VkShaderModule shaderModulesValues[2];
		shaderModulesValues[0] = CreateShaderModule(program.vkApp, );
		shaderModulesValues[1] = CreateShaderModule(program.vkApp, );
		Array<VkShaderModule> shaderModules;
		shaderModules.ptr = shaderModulesValues;
		shaderModules.length = 2;

		PipelineCreateInfo pipelineCreateInfo{};
		//pipelineCreateInfo.modules = shaderModules;
		pipelineCreateInfo.resolution = program.resolution;
		pipelineCreateInfo.renderPass = program.swapChainRenderPass;
		pipelineCreateInfo.getBindingDescriptions = Vertex2d::GetBindingDescriptions;
		pipelineCreateInfo.getAttributeDescriptions = Vertex2d::GetAttributeDescriptions;
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
