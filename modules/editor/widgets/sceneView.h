#pragma once

#include "editor\common.h"
#include "editor\editorPlugin.h"
#include "renderer\renderer.h"
#include "renderer\renderGraph.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API SceneView : public EditorWidget
    {
    public:
        explicit SceneView(EditorApp& app_);
        virtual ~SceneView();

        void Init();
        void Update(F32 dt);
        void EndFrame() override;
        void OnGUI() override;
        void Render() override;
        const char* GetName();

    private:
        EditorApp& app;
    };
}
} 
