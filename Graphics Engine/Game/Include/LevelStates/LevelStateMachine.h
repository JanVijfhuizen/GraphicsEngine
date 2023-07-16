#pragma once
#include "Levels/Level.h"

namespace game
{
	template <typename T>
	struct LevelState
	{
		virtual bool Create(T& state, const LevelCreateInfo& info) { return true; }
		virtual void Reset(T& state, const LevelInfo& info){}
		virtual bool Update(T& state, const LevelUpdateInfo& info, uint32_t& stateIndex, LevelIndex& loadLevelIndex) = 0;
	};

	template <typename T>
	struct LevelStateMachine final
	{
		T state{};
		uint32_t current = 0;
		LevelState<T>** states;
		uint32_t length;

		bool Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex);
		static LevelStateMachine Create(const LevelCreateInfo& info, const jv::Array<LevelState<T>*>& states, const T& state = {});
	};

	template <typename T>
	bool LevelStateMachine<T>::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		uint32_t index = current;
		const auto res = states[current]->Update(state, info, index, loadLevelIndex);
		if(index != current)
		{
			current = index;
			states[index]->Reset(state, info);
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
