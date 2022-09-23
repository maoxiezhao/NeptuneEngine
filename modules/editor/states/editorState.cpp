#include "editorState.h"
#include "editorStateMachine.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	const char* EditorState::GetName() const
	{
		return "";
	}

	EditorState::EditorState(EditorStateMachine& machine, EditorApp& editor_) :
		State(&machine),
		editor(editor_),
		stateMachine(machine)
	{
	}
}
}