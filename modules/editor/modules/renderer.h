#pragma once

#include "editorModule.h"

namespace VulkanTest
{
namespace Editor
{
	class RendererModule : public EditorModule
	{
	public:
		RendererModule(EditorApp& app);
		~RendererModule();

		virtual bool ShowComponentGizmo(WorldView& worldView, ECS::Entity entity, ECS::EntityID compID) = 0;
		virtual void OnEditingSceneChanged(Scene* newScene, Scene* prevScene) = 0;

		static RendererModule* Create(EditorApp& app);
	};
}
}
