#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API EntityListWidget : public EditorWidget
    {
    public:
        explicit EntityListWidget(EditorApp& editor_);
        virtual ~EntityListWidget();

        void Update(F32 dt);
        void OnGUI() override;
        const char* GetName();

    private:
        EditorApp& editor;
    };
}
}
