#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "client\app\app.h"
#include "renderer\renderer.h"
#include "renderer\renderPath3D.h"

namespace VulkanTest
{
namespace Editor
{
    class AssetCompiler;
    class EntityListWidget;

    class VULKAN_EDITOR_API EditorRenderer : public RenderPath3D
    {
    public:
        void SetViewportSize(U32 w, U32 h)
        {
            viewportSize = U32x2(w, h);
        }

        U32x2 GetViewportSize()const {
            return viewportSize;
        }

        void Render() override;

    protected:
        virtual U32x2 GetInternalResolution()const
        {
            return viewportSize;
        }

    private:
        U32x2 viewportSize = U32x2(0);
    };

    class VULKAN_EDITOR_API EditorApp : public App
    {
    public:
        static EditorApp* Create();
        static void Destroy(EditorApp* app);

        virtual void AddPlugin(EditorPlugin& plugin) = 0;
        virtual void AddWidget(EditorWidget& widget) = 0;
        virtual void RemoveWidget(EditorWidget& widget) = 0;

        virtual void AddWindow(Platform::WindowType window) = 0;
        virtual void RemoveWindow(Platform::WindowType window) = 0;
        virtual void DeferredDestroyWindow(Platform::WindowType window) = 0;

        virtual void SaveSettings() = 0;

        virtual EditorRenderer& GetEditorRenderer() = 0;
        virtual AssetCompiler& GetAssetCompiler() = 0;
        virtual EntityListWidget& GetEntityList() = 0;
    };
}   
} 
