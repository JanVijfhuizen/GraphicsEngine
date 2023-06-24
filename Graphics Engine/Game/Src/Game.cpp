#include "pch_game.h"
#include "Engine/InstancedRenderInterpreter.h"
#include "Engine/Engine.h"
#include "Tasks/InstancedRenderTask.h"

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	const auto scene = jv::ge::CreateScene();
	auto subArena = engine.CreateSubArena(1024);

	auto& renderTasks = engine.AddTaskSystem<game::InstancedRenderTask>();
	renderTasks.Allocate(subArena, 32);

	game::InstancedRenderInterpreterCreateInfo createInfo{};
	createInfo.resolution = jv::ge::GetResolution();

	game::InstancedRenderInterpreterEnableInfo enableInfo{};
	enableInfo.scene = scene;
	enableInfo.capacity = 32;

	auto& renderInterpreter = engine.AddTaskInterpreter<game::InstancedRenderTask,
		game::InstancedRenderInterpreter>(renderTasks, createInfo);
	renderInterpreter.Enable(enableInfo);

	while(true)
	{
		game::InstancedRenderTask renderTask{};
		renderTasks.Push(renderTask);

		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
