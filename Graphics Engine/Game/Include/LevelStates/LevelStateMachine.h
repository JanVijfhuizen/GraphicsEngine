#pragma once
#include "Levels/Level.h"

namespace game
{
	template <typename T>
	struct LevelState
	{
		virtual bool Create(T& state, const LevelCreateInfo& info) { return true; }
		virtual void Reset(T& state){}
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
		static LevelStateMachine Create(const LevelCreateInfo& info, const jv::Array<LevelState<T>*>& states);
	};

	template <typename T>
	bool LevelStateMachine<T>::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		uint32_t index = current;
		const auto res = states[current]->Update(state, info, index, loadLevelIndex);
		if(index != current)
		{
			current = index;
			states[index]->Reset(state);
		}
		return res;
	}

	template <typename T>
	LevelStateMachine<T> LevelStateMachine<T>::Create(const LevelCreateInfo& info, const jv::Array<LevelState<T>*>& states)
	{
		LevelStateMachine<T> stateMachine{};
		stateMachine.states = states.ptr;
		stateMachine.length = states.length;
		
		for (auto& state : states)
		{
			state->Create(stateMachine.state, info);
			state->Reset(stateMachine.state);
		}
		return stateMachine;
	}
}
