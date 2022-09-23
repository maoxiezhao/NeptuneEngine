#pragma once

#include "core\common.h"
#include "core\collections\array.h"

namespace VulkanTest
{
	class StateMachine;

	class State
	{
	public:
		State(StateMachine* machine);
		virtual ~State();
	
		virtual bool CanEnter()const {
			return true;
		}
		virtual bool CanExit(State* next)const {
			return true;
		}
		bool IsActive()const;

		virtual void OnEnter() {};
		virtual void OnExit() {};

	protected:
		friend class StateMachine;

		StateMachine* parent;
	};

	class StateMachine
	{
	public:
		StateMachine();
		~StateMachine();

		State* CurrentState() {
			return currState;
		}

		const State* CurrentState()const {
			return currState;
		}

		void GoToState(I32 stateIndex) 
		{
			ASSERT(stateIndex >= 0 && stateIndex < (I32)states.size());
			GoToState(states[stateIndex]);
		}

		void GoToState(State* state);

	protected:
		friend class State;

		virtual void SwitchState(State* next);

		Array<State*> states;
		State* currState = nullptr;
	};
}