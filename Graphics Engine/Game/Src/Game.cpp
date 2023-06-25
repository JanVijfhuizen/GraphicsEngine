#include "pch_game.h"
#include "Interpreters/InstancedRenderInterpreter.h"
#include "Engine/Engine.h"
#include "Interpreters/TextInterpreter.h"

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	const auto scene = jv::ge::CreateScene();
	auto subArena = engine.CreateSubArena(1024);

	auto& renderTasks = engine.AddTaskSystem<game::InstancedRenderTask>();
	renderTasks.Allocate(subArena, 32);

	auto& textTasks = engine.AddTaskSystem<game::TextTask>();
	textTasks.Allocate(subArena, 16);

	game::InstancedRenderInterpreterCreateInfo createInfo{};
	createInfo.resolution = jv::ge::GetResolution();

	game::InstancedRenderInterpreterEnableInfo enableInfo{};
	enableInfo.scene = scene;
	enableInfo.capacity = 32;

	game::TextInterpreterCreateInfo textInterpreterCreateInfo{};
	textInterpreterCreateInfo.instancedRenderTasks = &renderTasks;
	auto& textInterpreter = engine.AddTaskInterpreter<game::TextTask, game::TextInterpreter>(
		textTasks, textInterpreterCreateInfo);

	auto& renderInterpreter = engine.AddTaskInterpreter<game::InstancedRenderTask, game::InstancedRenderInterpreter>(
		renderTasks, createInfo);
	renderInterpreter.Enable(enableInfo);

	while(true)
	{
		static float f = 0;
		f += 0.01f;
		
		constexpr float dist = .2f;

		for (uint32_t i = 0; i < 5; ++i)
		{
			game::InstancedRenderTask renderTask{};
			renderTask.position.x = sin(f + dist * i) * .25f;
			renderTask.position.y = cos(f + dist * i) * .25f;
			renderTask.position *= 1.f + (5.f - static_cast<float>(i)) * .2f;
			renderTask.color.r *= static_cast<float>(i) / 5;
			renderTask.scale *= .2f;
			renderTasks.Push(renderTask);
		}

		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
