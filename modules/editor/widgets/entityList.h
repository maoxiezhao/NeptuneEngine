#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "editor\widgets\worldEditor.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    class WorldEditor;

    class VULKAN_EDITOR_API EntityListWidget : public EditorWidget
    {
    public:
        explicit EntityListWidget(EditorApp& editor_);
        virtual ~EntityListWidget();

        void Update(F32 dt);
        void OnGUI() override;
        const char* GetName();

    private:
        void OnFolderUI(EntityFolder& folder);
        void ShowHierarchy(ECS::EntityID entity, const Array<ECS::EntityID>& selectedEntities);

        EditorApp& editor;
        WorldEditor& worldEditor;
    };
}
}
