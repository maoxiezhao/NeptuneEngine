#pragma once

#include "editorState.h"

namespace VulkanTest
{
namespace Editor
{
	class EditingSceneState : public EditorState
	{
	public:
		friend class EditorStateMachine;

		const char* GetName() const override;

	protected:
		EditingSceneState(EditorStateMachine& machine, EditorApp& editor_);	
	};
}
}