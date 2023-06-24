#include "pch_game.h"

#include <stb_image.h>

#include "Engine/InstancedRenderInterpreter.h"
#include "Engine/Engine.h"
#include "Tasks/InstancedRenderTask.h"

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	const auto scene = jv::ge::CreateScene();

	auto& renderTasks = engine.AddTaskSystem<game::InstancedRenderTask>();

	game::InstancedRenderInterpreterCreateInfo createInfo{};
	createInfo.resolution = jv::ge::GetResolution();

	game::InstancedRenderInterpreterEnableInfo enableInfo{};
	enableInfo.scene = scene;
	enableInfo.capacity = 64;

	auto& renderInterpreter = engine.AddTaskInterpreter<game::InstancedRenderTask,
		game::InstancedRenderInterpreter>(renderTasks, createInfo);
	renderInterpreter.Enable(enableInfo);

	while(true)
	{
		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
