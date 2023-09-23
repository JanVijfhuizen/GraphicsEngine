#pragma once
#include "Levels/Level.h"

namespace game
{
	template <typename T>
	struct LevelState
	{
		virtual bool Create(T& state, const LevelCreateInfo& info) { return true; }
		virtual void Reset(T& state, const LevelInfo& info){}
		virtual bool Update(T& state, Level* level, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) = 0;
		virtual void OnExit(T& state, const LevelInfo& info){};
	};

	template <typename T>
	struct LevelStateMachine final
	{
		T state{};
		uint32_t current = 0;
		uint32_t next = -1;
		LevelState<T>** states;
		uint32_t length;

		bool Update(const LevelUpdateInfo& info, Level* level, LevelIndex& loadLevelIndex);
		static LevelStateMachine Create(const LevelCreateInfo& info, const jv::Array<LevelState<T>*>& states, const T& state = {});
	};

	template <typename T>
	bool LevelStateMachine<T>::Update(const LevelUpdateInfo& info, Level* level, LevelIndex& loadLevelIndex)
	{
		if(next != -1 && !level->GetIsLoading())
		{
			if (current != -1)
				states[current]->OnExit(state, info);
			current = next;
			next = -1;
			states[current]->Reset(state, info);
		}

		uint32_t index = current;
		const auto res = states[current]->Update(state, level, info, index, loadLevelIndex);
		if(index != current)
		{
			next = index;
			level->Load(LevelIndex::animOnly, true);
		}
		return res;
	}

	template <typename T>
	LevelStateMachine<T> LevelStateMachine<T>::Create(const LevelCreateInfo& info, const jv::Array<LevelState<T>*>& states, const T& state)
	{
		LevelStateMachine<T> stateMachine{};
		stateMachine.state = state;
		stateMachine.states = states.ptr;
		stateMachine.length = states.length;
		
		for (auto& levelState : states)
		{
			levelState->Create(stateMachine.state, info);
			levelState->Reset(stateMachine.state, info);
		}
		return stateMachine;
	}
}
