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
        static UniquePtr<ProfilerWidget> Create(EditorApp& app);
        virtual ~ProfilerWidget() {};
    };
}
}
