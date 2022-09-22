#include "stateMachine.h"

namespace VulkanTest
{
	State::State(StateMachine* machine_) :
		parent(machine_)
	{
		machine_->states.push_back(this);
	}

	State::~State()
	{
		if (parent)
			parent->states.erase(this);
	}

	bool State::IsActive() const
	{
		return parent->currState == this;
	}

	StateMachine::StateMachine() :
		currState(nullptr)
	{
	}

	StateMachine::~StateMachine()
	{
		for (auto state : states)
			state->parent = nullptr;
	}

	void StateMachine::GoToState(State* state)
	{
		if (state == currState)
			return;

		if (currState && !currState->CanExit(state))
			return;

		if (state && !state->CanEnter())
			return;

		SwitchState(state);
	}

	void StateMachine::SwitchState(State* next)
	{
		if (currState)
			currState->OnExit();

		currState = next;

		if (currState)
			currState->OnEnter();
	}
}