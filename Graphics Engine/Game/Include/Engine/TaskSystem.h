#pragma once
#include "JLib/VectorUtils.h"
#include "JLib/LinkedListUtils.h"

namespace game
{
	struct TaskSystemCreateInfo final
	{
		uint32_t chunkSize = 32;
	};

	class ITaskSystem
	{
	public:
		bool autoClear = true;
		virtual void ClearTasks() = 0;
	};

	template <typename T>
	class TaskSystem final : ITaskSystem
	{
		friend class Engine;

	public:
		void Push(const T& task);
		[[nodiscard]] jv::LinkedList<jv::Vector<T>> GetTaskBatches();
		void ClearTasks() override;

	private:
		uint32_t _chunkSize;
		jv::Arena* _frameArena;

		jv::LinkedListNode<jv::Vector<T>> _tasks{};
		jv::LinkedList<jv::Vector<T>> _additionalTasks{};

		[[nodiscard]] static TaskSystem Create(jv::Arena& frameArena, jv::Arena& arena, const TaskSystemCreateInfo& info);
		static void Destroy(jv::Arena& arena, const TaskSystem& taskSystem);
	};

	template <typename T>
	void TaskSystem<T>::Push(const T& task)
	{
		const auto batches = GetTaskBatches();

		bool fit = false;
		for (auto& batch : batches)
		{
			if (batch.count == batch.length)
				continue;

			batch.Add() = task;
			fit = true;
		}

		if (fit)
			return;
		if (_chunkSize == 0)
			return;

		auto& vector = Add(*_frameArena, _additionalTasks) = jv::CreateVector<T>(*_frameArena, _chunkSize);
		vector.Add() = task;
	}

	template <typename T>
	jv::LinkedList<jv::Vector<T>> TaskSystem<T>::GetTaskBatches()
	{
		jv::LinkedList<jv::Vector<T>> taskBatches{};
		_tasks.next = _additionalTasks.values;
		taskBatches.values = &_tasks;
		return taskBatches;
	}

	template <typename T>
	void TaskSystem<T>::ClearTasks()
	{
		_tasks.value.Clear();
		_additionalTasks = {};
	}

	template <typename T>
	TaskSystem<T> TaskSystem<T>::Create(jv::Arena& frameArena, jv::Arena& arena, const TaskSystemCreateInfo& info)
	{
		TaskSystem<T> taskSystem{};
		taskSystem._chunkSize = info.chunkSize;
		taskSystem._frameArena = &frameArena;
		taskSystem._tasks.value = jv::CreateVector<T>(arena, info.chunkSize);
		return taskSystem;
	}

	template <typename T>
	void TaskSystem<T>::Destroy(jv::Arena& arena, const TaskSystem& taskSystem)
	{
		jv::DestroyVector(arena, taskSystem._tasks.value);
	}
}
