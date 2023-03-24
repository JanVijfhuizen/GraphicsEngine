#include "pch.h"
#include "VkExamples/VkHelloWorld.h"

#include "Vk/VkImage.h"
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
		helloWorldProgram->image = texture::Load(program.vkCPUArena, program.vkGPUArena, program.vkApp, 
			imageCreateInfo, "ExampleArt/logo.png");

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
