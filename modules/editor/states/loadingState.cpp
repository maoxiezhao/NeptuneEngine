#include "loadingState.h"
#include "editor\editor.h"

namespace VulkanTest
{
namespace Editor
{
	LoadingState::LoadingState(EditorStateMachine& machine, EditorApp& editor_) :
		EditorState(machine, editor_)
	{
	}

	void LoadingState::Update()
	{
	}

	void LoadingState::InitFinished()
	{
		// Do somethings at the initialization ending
		// TODO ...

		// Editor finish initializationg
		editor.InitFinished();
	}

	const char* LoadingState::GetName() const
	{
		return "Loading";
	}
}
}