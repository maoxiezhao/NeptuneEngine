#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API RenderGraphWidget : public EditorWidget
    {
    public:
        static UniquePtr<RenderGraphWidget> Create(EditorApp& app);
        virtual ~RenderGraphWidget() {};
    };
}
}
