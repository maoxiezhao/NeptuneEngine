#pragma once

#include "core\utils\stateMachine.h"

#include "loadingState.h"
#include "editingSceneState.h"
#include "changingScenesState.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;

	enum class EditorStateType
	{
		Loading,
		Closing,
		EditingScene,
		ChangingScene,
		Count
	};

	class EditorStateMachine : public StateMachine
	{
	public:
		EditorStateMachine(EditorApp& editor_);
		~EditorStateMachine();

		void Update();
		void SwitchState(EditorStateType stateType);

		FORCE_INLINE EditorStateType GetCurrentStateType()const {
			return stateType;
		}

		const EditorState* CurrentState()const {
			return (EditorState*)StateMachine::CurrentState();
		}

		FORCE_INLINE EditorState* GetState(EditorStateType stateType) 
		{
			return editorStates[(I32)stateType];
		}

		FORCE_INLINE LoadingState* GetLoadingState() 
		{
			return (LoadingState*)GetState(EditorStateType::Loading);
		}

		FORCE_INLINE ChangingScenesState* GetChangingScenesState()
		{
			return (ChangingScenesState*)GetState(EditorStateType::ChangingScene);
		}

	private:
		void SwitchState(State* next) override;
		
		EditorApp& editor;
		Array<EditorState*> pendingStates;
		EditorState* editorStates[(I32)EditorStateType::Count];
		EditorStateType stateType = EditorStateType::EditingScene;
	};
}
}
