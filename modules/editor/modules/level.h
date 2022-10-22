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

	class LevelModule : public EditorModule
	{
	public:
		LevelModule(EditorApp& app);
		~LevelModule();

		static LevelModule* Create(EditorApp& app);

		virtual void OpenScene(const Path& path) = 0;
		virtual void OpenScene(const Guid& guid) = 0;
		virtual void SaveScene(Scene* scene) = 0;
		virtual void CloseScene(Scene* scene) = 0;

		virtual void SaveEditingScene() = 0;
		virtual void SaveAllScenes() = 0;
		virtual Array<Scene*>& GetLoadedScenes() = 0;
		virtual void EditScene(Scene* scene) = 0;
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
