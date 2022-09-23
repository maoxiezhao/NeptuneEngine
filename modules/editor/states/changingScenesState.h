#pragma once

#include "editorState.h"
#include "level\level.h"

namespace VulkanTest
{
namespace Editor
{
	class ChangingScenesState : public EditorState
	{
	public:
		friend class EditorStateMachine;

		void OnEnter() override;
		void OnExit() override;
		const char* GetName() const override;

		void LoadScene(const Guid& guid);
		void UnloadScene(Scene* scene);
		void TryEnter();

	protected:
		ChangingScenesState(EditorStateMachine& machine, EditorApp& editor_);
		virtual ~ChangingScenesState();

		void OnSceneEventCallback(Scene* scene, const Guid& sceneID);

	private:
		Array<Guid> toLoadScenes;
		Array<Scene*> toUnloadScenes;
		Guid lastRequestScene = Guid::Empty;
	};
}
}
