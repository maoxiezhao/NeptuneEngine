#pragma once

#include "editor\common.h"
#include "core\filesystem\filesystem.h"
#include "level\scene.h"

#ifdef STATIC_PLUGINS
#define VULKAN_EDITOR_ENTRY(plugin_name) \
		extern "C" VulkanTest::Editor::EditorPlugin* setEditor_##plugin_name(VulkanTest::Editor::EditorApp& app); \
		extern "C" VulkanTest::Editor::EditorPlugin* setEditor_##plugin_name(VulkanTest::Editor::EditorApp& app)
#else
#define VULKAN_EDITOR_ENTRY(plugin_name) \
		extern "C" VulkanTest::Editor::EditorPlugin* setEditorApp(VulkanTest::Editor::EditorApp& app)
#endif

namespace VulkanTest
{
namespace Editor
{
    struct WorldView;

    struct EditorPlugin
    {
        virtual ~EditorPlugin() {}
        virtual void Initialize() = 0;
        virtual const char* GetName()const = 0;
        virtual bool ShowComponentGizmo(WorldView& worldView, ECS::Entity entity, ECS::EntityID compID) { return false; }
        virtual void OnEditingSceneChanged(Scene* newScene, Scene* prevScene) {};
    };

    struct EditorWidget
    {
        virtual ~EditorWidget() {}

        virtual void InitFinished() {};
        virtual void Update(F32 dt) {}
        virtual void LateUpdate() {}
        virtual void EndFrame() {}
        virtual void OnGUI() = 0;
        virtual void Render() {};
        virtual bool HasFocus() { return false; }
        virtual void OnEditingSceneChanged(Scene* newScene, Scene* prevScene) {}
        virtual const char* GetName() = 0;

        bool IsOpen()const {
            return isOpen;
        }
        bool isOpen = true;
    };

    struct RenderInterface
    {
        virtual~RenderInterface() = default;

        virtual void* LoadTexture(const Path& path) = 0;
        virtual void* CreateTexture(const char* name, const void* pixels, int w, int h) = 0;
        virtual void  DestroyTexture(void* handle) = 0;
    };
} 
} 
