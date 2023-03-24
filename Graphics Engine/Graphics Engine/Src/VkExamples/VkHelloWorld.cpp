#include "pch.h"
#include "VkExamples/VkHelloWorld.h"

#include "VkHL/VkMesh.h"
#include "VkHL/VkShapes.h"

namespace jv::vk::example
{
	struct HelloWorldProgram final
	{
		uint64_t scope;
		Mesh mesh;
	};

	void* OnBegin(Program& program)
	{
		const auto helloWorldProgram = program.arena.New<HelloWorldProgram>();
		helloWorldProgram->scope = program.arena.CreateScope();

		Array<Vertex2d> vertices;
		Array<VertexIndex> indices;
		const auto scope = CreateQuadShape(program.tempArena, vertices, indices);

		helloWorldProgram->mesh = Mesh::Create(program.vkCPUArena, program.vkGPUArena, program.vkApp, vertices, indices);

		program.tempArena.DestroyScope(scope);

		return helloWorldProgram;
	}

	bool OnUpdate(Program& program, void* userPtr)
	{
		return true;
	}

	bool OnExit(Program& program, void* userPtr)
	{
		const auto helloWorldProgram = static_cast<HelloWorldProgram*>(userPtr);

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
