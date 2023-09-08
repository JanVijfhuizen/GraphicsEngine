#pragma once
#include "Engine/Engine.h"
#include "Tasks/RenderTask.h"

namespace game
{
	template <typename T>
	struct PriorityInterpreterCreateInfo final
	{
		TaskSystem<T>* tasks = nullptr;
	};

	template <typename T>
	class PriorityInterpreter final : public TaskInterpreter<T, PriorityInterpreterCreateInfo<T>>
	{
		TaskSystem<T>* _tasks;

		void OnStart(const PriorityInterpreterCreateInfo<T>& createInfo, const EngineMemory& memory) override;
		void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<T>>& tasks) override;
		void OnExit(const EngineMemory& memory) override;
	};

	template <typename T>
	void PriorityInterpreter<T>::OnStart(const PriorityInterpreterCreateInfo<T>& createInfo,
		const EngineMemory& memory)
	{
		_tasks = createInfo.tasks;
	}

	template <typename T>
	void PriorityInterpreter<T>::OnUpdate(const EngineMemory& memory,
		const jv::LinkedList<jv::Vector<T>>& tasks)
	{
		for (const auto& batch : tasks)
			for (const auto& task : batch)
				_tasks->Push(task);
	}

	template <typename T>
	void PriorityInterpreter<T>::OnExit(const EngineMemory& memory)
	{
	}
}
