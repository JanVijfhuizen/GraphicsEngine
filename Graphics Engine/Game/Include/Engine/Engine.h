#pragma once
#include "TaskSystem.h"

namespace game
{
	struct EngineCreateInfo final
	{
		uint32_t arenaSize = 4096;
		uint32_t tempArenaSize = 4096;
		uint32_t frameArenaSize = 4096;
	};

	class ITaskInterpreter
	{
		friend struct Engine;

	protected:
		void* GetTaskSystemPtr() const;

	private:
		void* _taskSystem;
		virtual void Start(uint32_t chunkCapacity) = 0;
		virtual void Update() = 0;
		virtual void Exit() = 0;
	};

	template <typename T>
	class TaskInterpreter : public ITaskInterpreter
	{
	protected:
		virtual void OnStart(uint32_t chunkCapacity) = 0;
		virtual void OnUpdate(const jv::LinkedList<jv::Vector<T>>& tasks) = 0;
		virtual void OnExit() = 0;
	private:
		void Start(uint32_t chunkCapacity) override;
		void Update() override;
		void Exit() override;
	};

	struct Engine final
	{
		void* arenaMem;
		void* tempArenaMem;
		void* frameArenaMem;
		jv::Arena arena;
		jv::Arena tempArena;
		jv::Arena frameArena;
		jv::LinkedList<ITaskSystem*> taskSystems{};
		jv::LinkedList<ITaskInterpreter*> taskInterpreters{};

		template <typename T>
		[[nodiscard]] TaskSystem<T>& AddTaskSystem();
		template <typename Task, typename Interpreter>
		[[nodiscard]] Interpreter& AddTaskInterpreter(TaskSystem<Task>& taskSystem);
		[[nodiscard]] bool Update();

		[[nodiscard]] static Engine Create(const EngineCreateInfo& info);
		static void Destroy(const Engine& engine);
	};

	template <typename T>
	TaskSystem<T>& Engine::AddTaskSystem()
	{
		auto sys = arena.New<TaskSystem<T>>();
		Add(arena, taskSystems) = sys;
		return *sys;
	}

	template <typename T>
	void TaskInterpreter<T>::Start(const uint32_t chunkCapacity)
	{
		OnStart(chunkCapacity);
	}

	template <typename T>
	void TaskInterpreter<T>::Update()
	{
		auto taskSystem = static_cast<TaskSystem<T>*>(GetTaskSystemPtr());
		auto batches = taskSystem->GetTaskBatches();
		OnUpdate(batches);
	}

	template <typename T>
	void TaskInterpreter<T>::Exit()
	{
		OnExit();
	}

	template <typename Task, typename Interpreter>
	Interpreter& Engine::AddTaskInterpreter(TaskSystem<Task>& taskSystem)
	{
		auto taskInterpreter = arena.New<Interpreter>();
		Add(arena, taskInterpreters) = taskInterpreter;
		taskInterpreter->_taskSystem = &taskSystem;
		return *taskInterpreter;
	}
}
