#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    class WorldEditor;

    class VULKAN_EDITOR_API PropertyWidget : public EditorWidget
    {
    public:
        struct IPlugin
        {
            virtual ~IPlugin() {}
            virtual void Update() {}
            virtual void OnGUI(PropertyWidget& property) = 0;
        };

        explicit PropertyWidget(EditorApp& editor_);
        virtual ~PropertyWidget();

        void Update(F32 dt);
        void OnGUI() override;
        const char* GetName();

    private:
        void ShowBaseProperties(ECS::Entity entity);
        void ShowComponentProperties(ECS::Entity entity, ECS::EntityID compID);

        EditorApp& editor;
        WorldEditor& worldEditor;

        char componentFilter[32];
    };
}
}
