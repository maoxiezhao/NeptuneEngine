#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "client\app\app.h"

namespace VulkanTest
{
namespace Editor
{
    class VULKAN_EDITOR_API EditorApp
    {
    public:
        static EditorApp* Create();
        static void Destroy(EditorApp* app);

        virtual void AddPlugin(EditorPlugin& plugin) = 0;
        virtual void AddWidget(EditorWidget& widget) = 0;
        virtual void RemoveWidget(EditorWidget& widget) = 0;
    };
}   
} 
