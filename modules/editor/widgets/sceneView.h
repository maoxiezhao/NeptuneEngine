#pragma once

#include "editor\common.h"
#include "editor\editorUtils.h"
#include "editor\editorPlugin.h"
#include "editor\widgets\worldEditor.h"
#include "renderer\renderer.h"
#include "renderer\renderGraph.h"
#include "renderer\renderPath3D.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;

    class VULKAN_EDITOR_API EditorRenderer : public RenderPath3D
    {
    public:
        void SetViewportSize(U32 w, U32 h) {
            viewportSize = U32x2(w, h);
        }

        U32x2 GetViewportSize()const {
            return viewportSize;
        }

        void Update(F32 dt) override;
        void Render() override;

    protected:
        virtual U32x2 GetInternalResolution()const {
            return viewportSize;
        }

    private:
        U32x2 viewportSize = U32x2(0);
    };

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

        EditorRenderer& GetRenderer() {
            return *editorRenderer;
        }

    private:
        void HandleEvents();
        void CaptureMouse(bool capture);
        I32x2 GetLocalMousePoint()const;

    private:
        EditorApp& app;
        struct WorldViewImpl* worldView = nullptr;
        I32x2 screenPos;
        I32x2 screenSize;
        bool isMouseCaptured = false;
        I32x2 capturedMousePos = I32x2(0, 0);
        F32 cameraSpeed = 0.5f;
        UniquePtr<EditorRenderer> editorRenderer;
        WorldEditor& worldEditor;

        Utils::Action moveForwardAction;
        Utils::Action moveBackAction;
        Utils::Action moveLeftAction;
        Utils::Action moveRightAction;
    };
}
} 
