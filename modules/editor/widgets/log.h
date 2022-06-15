#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API LogWidget : public EditorWidget
    {
    public:
        explicit LogWidget();
        virtual ~LogWidget();

        void Update(F32 dt);
        void OnGUI() override;
        const char* GetName();

    private:
        bool isOpen = true;
    };
}
}
