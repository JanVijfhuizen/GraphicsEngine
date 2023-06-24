#include "pch_game.h"

#include "Engine/BasicRenderInterpreter.h"
#include "Engine/Engine.h"
#include "Tasks/RenderTask.h"

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	const auto scene = jv::ge::CreateScene();

	auto& renderTasks = engine.AddTaskSystem<game::RenderTask>();

	game::BasicRenderInterpreterCreateInfo basicRenderInterpreterCreateInfo{};
	basicRenderInterpreterCreateInfo.resolution = jv::ge::GetResolution();
	basicRenderInterpreterCreateInfo.frameCount = jv::ge::GetFrameCount();
	basicRenderInterpreterCreateInfo.scene = scene;
	basicRenderInterpreterCreateInfo.capacity = 64;

	auto& testInterpreter = engine.AddTaskInterpreter<game::RenderTask,
		game::BasicRenderInterpreter>(renderTasks, basicRenderInterpreterCreateInfo);

	while(true)
	{
		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
