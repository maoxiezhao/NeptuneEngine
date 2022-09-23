#pragma once

#include "core\utils\stateMachine.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;
	class EditorStateMachine;

	class EditorState : public State
	{
	public:
		virtual void Update() {}
		virtual const char* GetName()const;

		// Editor status
		virtual bool CanChangeScene()const {  
			return true;
		}

	protected:
		EditorState(EditorStateMachine& machine, EditorApp& editor_);
		EditorApp& editor;
		EditorStateMachine& stateMachine;
	};
}
}
