#pragma once

#include "editorState.h"

namespace VulkanTest
{
namespace Editor
{
	class LoadingState : public EditorState
	{
	public:
		friend class EditorStateMachine;

		void Update() override;
		const char* GetName() const override;
		void InitFinished();

	protected:
		LoadingState(EditorStateMachine& machine, EditorApp& editor_);		
	};
}
}
