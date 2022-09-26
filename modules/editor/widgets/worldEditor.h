#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "core\scene\world.h"
#include "renderer\renderScene.h"
#include "level\level.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    struct EntityFolder;

    struct IEditorCommand
    {
        virtual ~IEditorCommand() {}

        virtual bool Execute() = 0;
        virtual void Undo() = 0;
        virtual const char* GetType() = 0;
    };
   
    struct EditingScene
    {
        Scene* scene = nullptr;
        EntityFolder* folder = nullptr;
        bool isEdited = true;
    };

    struct VULKAN_EDITOR_API WorldView
    {
        virtual ~WorldView() = default;

        virtual bool IsMouseDown(Platform::MouseButton button) const = 0;
        virtual bool IsMouseClick(Platform::MouseButton button) const = 0;
        virtual F32x2 GetMousePos() const = 0;
        virtual const CameraComponent& GetCamera()const = 0;
        virtual World* GetWorld()const = 0;
    };

    class VULKAN_EDITOR_API WorldEditor : public EditorWidget
    {
    public:
        static UniquePtr<WorldEditor> Create(EditorApp& app);
        virtual ~WorldEditor() {};

        virtual World* GetWorld() = 0;
        virtual Array<Scene*>& GetScenes() = 0;
        virtual Scene* GetEditingScene() = 0;
        virtual void SetEditingScene(Scene* scene) = 0;
        virtual void OpenScene(Scene* scene) = 0;
        virtual void CloseScene(Scene* scene) = 0;
        virtual void SaveEditingScene() = 0;
        virtual void SaveAllScenes() = 0;

        virtual WorldView& GetView() = 0;
        virtual void SetView(WorldView& view_) = 0;
        virtual EntityFolder* GetEntityFolder() = 0;

        virtual void ClearSelectEntities() = 0;
        virtual void SelectEntities(Span<const ECS::Entity> entities, bool toggle) = 0;
        virtual const Array<ECS::Entity>& GetSelectedEntities() const = 0;

        // Editor command
        virtual void ExecuteCommand(UniquePtr<IEditorCommand>&& command) = 0;
        virtual ECS::Entity AddEmptyEntity() = 0;
        virtual ECS::Entity DeleteEntity(ECS::Entity entity) = 0;
        virtual void AddComponent(ECS::Entity entity, ComponentType compType) = 0;
    };
}
}