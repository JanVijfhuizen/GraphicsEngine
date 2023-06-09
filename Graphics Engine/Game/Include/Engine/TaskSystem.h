#pragma once
#include "JLib/VectorUtils.h"
#include "JLib/LinkedListUtils.h"

namespace game
{
	struct TaskSystemCreateInfo final
	{
		uint32_t taskChunkSize = 32;
	};

	struct ITaskSystem
	{
		bool autoClear = true;
		virtual void ClearTasks() = 0;
	};

	template <typename T>
	struct TaskSystem final : ITaskSystem
	{
		uint32_t chunkSize;
		jv::LinkedListNode<jv::Vector<T>> tasks{};
		jv::LinkedList<jv::Vector<T>> additionalTasks{};

		void Push(jv::Arena& frameArena, const T& task);
		[[nodiscard]] jv::LinkedList<jv::Vector<T>> GetTaskBatches();
		void ClearTasks() override;

		[[nodiscard]] static TaskSystem Create(jv::Arena& arena, const TaskSystemCreateInfo& info);
		static void Destroy(jv::Arena& arena, const TaskSystem& taskSystem);
	};

	template <typename T>
	void TaskSystem<T>::Push(jv::Arena& frameArena, const T& task)
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
		if (chunkSize == 0)
			return;

		auto& vector = Add(frameArena, additionalTasks) = jv::CreateVector<T>(frameArena, chunkSize);
		vector.Add() = task;
	}

	template <typename T>
	jv::LinkedList<jv::Vector<T>> TaskSystem<T>::GetTaskBatches()
	{
		jv::LinkedList<jv::Vector<T>> taskBatches{};
		tasks.next = additionalTasks.values;
		taskBatches.values = &tasks;
		return taskBatches;
	}

	template <typename T>
	void TaskSystem<T>::ClearTasks()
	{
		tasks.value.Clear();
		additionalTasks = {};
	}

	template <typename T>
	TaskSystem<T> TaskSystem<T>::Create(jv::Arena& arena, const TaskSystemCreateInfo& info)
	{
		TaskSystem<T> taskSystem{};
		taskSystem.chunkSize = info.taskChunkSize;
		taskSystem.tasks.value = jv::CreateVector<T>(arena, info.taskChunkSize);
		return taskSystem;
	}

	template <typename T>
	void TaskSystem<T>::Destroy(jv::Arena& arena, const TaskSystem& taskSystem)
	{
		jv::DestroyVector(arena, taskSystem.tasks.value);
	}
}
