#include "editingSceneState.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	EditingSceneState::EditingSceneState(EditorStateMachine& machine, EditorApp& editor_) :
		EditorState(machine, editor_)
	{
	}

	const char* EditingSceneState::GetName() const
	{
		return "EditingScene";
	}
}
}