#pragma once

#include "editor\common.h"
#include "editor\editorUtils.h"
#include "editor\editorPlugin.h"
#include "client\app\app.h"
#include "renderer\renderer.h"

struct ImFont;

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    namespace Gizmo { struct Config; }

    struct IAddComponentPlugin
    {
        virtual ~IAddComponentPlugin() {}
        virtual void OnGUI(bool createEntity, bool fromFilter, class WorldEditor& editor) = 0;
        virtual const char* GetLabel() const = 0;
    };

    struct AddComponentTreeNode
    {
        IAddComponentPlugin* plugin = nullptr;
        AddComponentTreeNode* child = nullptr;
        AddComponentTreeNode* next = nullptr;

        char label[64];
    };

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
        virtual void SetRenderInterace(RenderInterface* renderInterface_) = 0;
        virtual void RegisterComponent(const char* icon, ECS::EntityID compID, IAddComponentPlugin* plugin) = 0;

        virtual EditorPlugin* GetPlugin(const char* name) = 0;
        virtual Utils::Action* GetAction(const char* name) = 0;
        virtual const Array<Platform::WindowEvent>& GetWindowEvents()const = 0;
        virtual RenderInterface* GetRenderInterface() = 0;
        virtual class AssetCompiler& GetAssetCompiler() = 0;
        virtual class AssetBrowser& GetAssetBrowser() = 0;
        virtual class EntityListWidget& GetEntityList() = 0;
        virtual class WorldEditor& GetWorldEditor() = 0;
        virtual Gizmo::Config& GetGizmoConfig() = 0;
        virtual ImFont* GetBigIconFont() = 0;
        virtual ImFont* GetBoldFont() = 0;
        virtual const AddComponentTreeNode* GetAddComponentTreeNodeRoot()const = 0;
        virtual class ProfilerTools& GetProfilerTools() = 0;

        virtual const char* GetComponentIcon(ECS::EntityID compID) const = 0;
        virtual const char* GetComponentTypeName(ECS::EntityID compID) const = 0;
    };
}   
} 
