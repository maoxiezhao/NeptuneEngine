#pragma once

#include "editor\common.h"
#include "editor\editorUtils.h"
#include "editor\editorPlugin.h"
#include "client\app\app.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
namespace Editor
{
    class VULKAN_EDITOR_API EditorApp : public App
    {
    public:
        static EditorApp* Create();
        static void Destroy(EditorApp* app);

        virtual void AddPlugin(EditorPlugin& plugin) = 0;
        virtual void AddWidget(EditorWidget& widget) = 0;
        virtual EditorWidget* GetWidget(const char* name) = 0;
        virtual void RemoveWidget(EditorWidget& widget) = 0;

        virtual void AddWindow(Platform::WindowType window) = 0;
        virtual void RemoveWindow(Platform::WindowType window) = 0;
        virtual void DeferredDestroyWindow(Platform::WindowType window) = 0;

        virtual void AddAction(Utils::Action* action) = 0;
        virtual void RemoveAction(Utils::Action* action) = 0;
        virtual Array<Utils::Action*>& GetActions() = 0;
        virtual void SetCursorCaptured(bool capture) = 0;
        virtual void SaveSettings() = 0;

        virtual const Array<Platform::WindowEvent>& GetWindowEvents()const = 0;
        virtual class AssetCompiler& GetAssetCompiler() = 0;
        virtual class EntityListWidget& GetEntityList() = 0;
        virtual class WorldEditor& GetWorldEditor() = 0;
    };
}   
} 
