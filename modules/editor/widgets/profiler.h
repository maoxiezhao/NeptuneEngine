#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API ProfilerWidget : public EditorWidget
    {
    public:
        explicit ProfilerWidget(EditorApp& editor_);
        virtual ~ProfilerWidget();

        void Update(F32 dt) override;
        void OnGUI() override;
        const char* GetName();

    private:
        EditorApp& editor;
    };
}
}
