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
		~State();
	
		virtual bool CanEnter()const {
			return true;
		}
		virtual bool CanExit(State* next)const {
			return true;
		}

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

		void SwitchState(State* next);

		State* CurrentState() {
			return currState;
		}

		const State* CurrentState()const {
			return currState;
		}

	protected:
		Array<State*> states;
		State* currState = nullptr;
	};
}