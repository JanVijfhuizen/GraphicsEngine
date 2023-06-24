#pragma once
#include "TaskSystem.h"

namespace game
{
	struct EngineMemory final
	{
		friend class Engine;

		jv::Arena& arena;
		jv::Arena& tempArena;
		jv::Arena& frameArena;

	private:
		EngineMemory(jv::Arena& arena, jv::Arena& tempArena, jv::Arena& frameArena);
	};

	struct EngineCreateInfo final
	{
		uint32_t arenaSize = 4096;
		uint32_t tempArenaSize = 4096;
		uint32_t frameArenaSize = 4096;
	};

	class ITaskInterpreter
	{
		friend class Engine;

	protected:
		[[nodiscard]] void* GetTaskSystemPtr() const;

	private:
		void* _taskSystem;
		
		virtual void Update(const EngineMemory& memory) = 0;
		virtual void Exit(const EngineMemory& memory) = 0;
	};

	template <typename Task, typename CreateInfo>
	class TaskInterpreter : public ITaskInterpreter
	{
		friend class Engine;

	protected:
		virtual void OnStart(const CreateInfo& createInfo, const EngineMemory& memory) = 0;
		virtual void OnUpdate(const EngineMemory& memory, const jv::LinkedList<jv::Vector<Task>>& tasks) = 0;
		virtual void OnExit(const EngineMemory& memory) = 0;
	private:
		void Update(const EngineMemory& memory) override;
		void Exit(const EngineMemory& memory) override;
	};

	class Engine final
	{
	public:
		template <typename T>
		[[nodiscard]] TaskSystem<T>& AddTaskSystem();
		template <typename Task, typename Interpreter, typename CreateInfo>
		[[nodiscard]] Interpreter& AddTaskInterpreter(TaskSystem<Task>& taskSystem, const CreateInfo& createInfo);
		[[nodiscard]] bool Update();

		[[nodiscard]] static Engine Create(const EngineCreateInfo& info);
		static void Destroy(const Engine& engine);
		[[nodiscard]] EngineMemory GetMemory();
		[[nodiscard]] jv::Arena CreateSubArena(uint32_t size);

	private:
		void* _arenaMem;
		void* _tempArenaMem;
		void* _frameArenaMem;
		jv::Arena _arena;
		jv::Arena _tempArena;
		jv::Arena _frameArena;
		jv::LinkedList<ITaskSystem*> _taskSystems{};
		jv::LinkedList<ITaskInterpreter*> _taskInterpreters{};
	};

	template <typename Task, typename CreateInfo>
	void TaskInterpreter<Task, CreateInfo>::Update(const EngineMemory& memory)
	{
		auto taskSystem = static_cast<TaskSystem<Task>*>(GetTaskSystemPtr());
		auto batches = taskSystem->GetTaskBatches();
		OnUpdate(memory, batches);
	}

	template <typename Task, typename CreateInfo>
	void TaskInterpreter<Task, CreateInfo>::Exit(const EngineMemory& memory)
	{
		OnExit(memory);
	}

	template <typename T>
	TaskSystem<T>& Engine::AddTaskSystem()
	{
		auto sys = _arena.New<TaskSystem<T>>();
		*sys = TaskSystem<T>::Create(_frameArena);
		Add(_arena, _taskSystems) = sys;
		return *sys;
	}

	template <typename Task, typename Interpreter, typename CreateInfo>
	Interpreter& Engine::AddTaskInterpreter(TaskSystem<Task>& taskSystem, const CreateInfo& createInfo)
	{
		auto taskInterpreter = _arena.New<Interpreter>();
		Add(_arena, _taskInterpreters) = taskInterpreter;

		TaskInterpreter<Task, CreateInfo>* ptr = taskInterpreter;
		ptr->_taskSystem = &taskSystem;
		ptr->OnStart(createInfo, GetMemory());
		return *taskInterpreter;
	}
}
