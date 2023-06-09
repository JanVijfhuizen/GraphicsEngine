#include "pch_game.h"
#include "Engine/Engine.h"
#include "Tasks/RenderTask.h"

class TestInterpreter : public game::TaskInterpreter<game::RenderTask>
{
	void OnStart(uint32_t chunkCapacity) override;
	void OnUpdate(const jv::LinkedList<jv::Vector<game::RenderTask>>& tasks) override;
	void OnExit() override;
};

void TestInterpreter::OnStart(uint32_t chunkCapacity)
{
}

void TestInterpreter::OnUpdate(const jv::LinkedList<jv::Vector<game::RenderTask>>& tasks)
{
}

void TestInterpreter::OnExit()
{
}

int main()
{
	game::EngineCreateInfo engineCreateInfo{};
	auto engine = game::Engine::Create(engineCreateInfo);

	auto& renderTasks = engine.AddTaskSystem<game::RenderTask>();
	auto& testInterpreter = engine.AddTaskInterpreter<game::RenderTask, TestInterpreter>(renderTasks);

	while(true)
	{
		const bool result = engine.Update();
		if (!result)
			break;
	}

	game::Engine::Destroy(engine);
}
