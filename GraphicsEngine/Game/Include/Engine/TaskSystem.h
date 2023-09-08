#pragma once
#include "JLib/VectorUtils.h"
#include "JLib/LinkedListUtils.h"

namespace game
{
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
		void Allocate(jv::Arena& arena, uint32_t chunkSize);

		void Push(const T& task);
		[[nodiscard]] jv::LinkedList<jv::Vector<T>> GetTaskBatches();
		void ClearTasks() override;

	private:
		uint32_t _chunkSize = 0;
		jv::Arena* _frameArena;

		jv::LinkedListNode<jv::Vector<T>> _tasks{};
		jv::LinkedList<jv::Vector<T>> _additionalTasks{};

		[[nodiscard]] static TaskSystem Create(jv::Arena& frameArena);
		static void Destroy(jv::Arena& arena, const TaskSystem& taskSystem);
	};

	template <typename T>
	void TaskSystem<T>::Allocate(jv::Arena& arena, const uint32_t chunkSize)
	{
		_chunkSize = chunkSize;
		_tasks.value = jv::CreateVector<T>(arena, chunkSize);
		_additionalTasks = {};
	}

	template <typename T>
	void TaskSystem<T>::Push(const T& task)
	{
		assert(_chunkSize > 0);
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
	TaskSystem<T> TaskSystem<T>::Create(jv::Arena& frameArena)
	{
		TaskSystem<T> taskSystem{};
		taskSystem._frameArena = &frameArena;
		return taskSystem;
	}

	template <typename T>
	void TaskSystem<T>::Destroy(jv::Arena& arena, const TaskSystem& taskSystem)
	{
		jv::DestroyVector(arena, taskSystem._tasks.value);
	}
}
