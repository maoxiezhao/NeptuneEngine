#include "editorStateMachine.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	EditorStateMachine::EditorStateMachine(EditorApp& editor_) :
		editor(editor_)
	{
		memset(editorStates, 0, sizeof(editorStates));
		editorStates[(I32)EditorStateType::Loading] = CJING_NEW(LoadingState)(*this, editor_);
		editorStates[(I32)EditorStateType::EditingScene] = CJING_NEW(EditingSceneState)(*this, editor_);
		editorStates[(I32)EditorStateType::ChangingScene] = CJING_NEW(ChangingScenesState)(*this, editor_);

		SwitchState(EditorStateType::Loading);
	}

	EditorStateMachine::~EditorStateMachine()
	{
		for (int i = 0; i < ARRAYSIZE(editorStates); i++)
			CJING_SAFE_DELETE(editorStates[i]);
	}

	void EditorStateMachine::Update()
	{
		while (!pendingStates.empty())
		{
			auto state = pendingStates.back();
			pendingStates.pop_back();
			GoToState(state);
		}

		if (currState != nullptr)
		{
			EditorState* state = (EditorState*)currState;
			state->Update();
		}
	}

	void EditorStateMachine::SwitchState(EditorStateType stateType_)
	{
		stateType = stateType_;
		GoToState(editorStates[(I32)stateType]);
	}

	void EditorStateMachine::SwitchState(State* next)
	{
		Logger::Print("Changing editor state from %s to %s",
			currState ? ((EditorState*)currState)->GetName() : "Null",
			next ? ((EditorState*)next)->GetName() : "Null");
		StateMachine::SwitchState(next);
	}
}
}