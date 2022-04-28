#pragma once

namespace VulkanTest
{
namespace Editor
{
    struct EditorPlugin
    {
        virtual ~EditorPlugin() {}
        virtual void Initialize() = 0;
        virtual void Uninitialize() = 0;
        virtual const char* GetName()const = 0;
    };

    struct EditorWidget
    {
        virtual ~EditorWidget() {}

        virtual void Update(F32 dt) {}
        virtual void EndFrame() {}
        virtual void OnGUI() = 0;
        virtual const char* GetName() = 0;
    };
} 
} 
