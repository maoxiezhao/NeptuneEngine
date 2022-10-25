#pragma once

#include "editorModule.h"

namespace VulkanTest
{
namespace Editor
{
	class EditorApp;
	struct WorldView;

	struct IEditorCommand
	{
		virtual ~IEditorCommand() {}

		virtual bool Execute() = 0;
		virtual void Undo() = 0;
		virtual const char* GetType() = 0;
	};

	class SceneEditingModule : public EditorModule
	{
	public:
		SceneEditingModule(EditorApp& app);
		~SceneEditingModule();

		static SceneEditingModule* Create(EditorApp& app);

		virtual void SaveEditingScene() = 0;
		virtual void EditScene(Scene* scene) = 0;
		virtual void CloseEditingScene() = 0;
		virtual Scene* GetEditingScene() = 0;
		virtual World* GetEditingWorld() = 0;

		virtual void ClearSelectEntities() = 0;
		virtual void SelectEntities(Span<const ECS::Entity> entities, bool toggle) = 0;
		virtual const Array<ECS::Entity>& GetSelectedEntities() const = 0;

		virtual void ExecuteCommand(UniquePtr<IEditorCommand>&& command) = 0;
		virtual ECS::Entity AddEmptyEntity() = 0;
		virtual ECS::Entity DeleteEntity(ECS::Entity entity) = 0;
		virtual void AddComponent(ECS::Entity entity, ComponentType compType) = 0;
		virtual void MakeParent(ECS::Entity parent, ECS::Entity child) = 0;

		virtual WorldView& GetView() = 0;
		virtual void SetView(WorldView& view_) = 0;
	};
}
}
