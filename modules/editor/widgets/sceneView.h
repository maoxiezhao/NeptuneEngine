#pragma once

#include "editor\common.h"
#include "editor\editorUtils.h"
#include "editor\editorPlugin.h"
#include "editor\widgets\editorIcons.h"
#include "editor\widgets\assetItem.h"
#include "renderer\renderer.h"
#include "renderer\renderGraph.h"
#include "renderer\renderPath3D.h"
#include "renderer\render2D\font.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorApp;
    class SceneView;

    struct VULKAN_EDITOR_API WorldView
    {
        virtual ~WorldView() = default;

        virtual bool IsMouseDown(Platform::MouseButton button) const = 0;
        virtual bool IsMouseClick(Platform::MouseButton button) const = 0;
        virtual F32x2 GetMousePos() const = 0;
        virtual const CameraComponent& GetCamera()const = 0;
        virtual World* GetWorld()const = 0;
    };

    class VULKAN_EDITOR_API EditorRenderer : public RenderPath3D
    {
    public:
        EditorRenderer(EditorApp& editor_, SceneView& sceneView_);

        void SetViewportSize(U32 w, U32 h) {
            viewportSize = U32x2(w, h);
        }

        U32x2 GetViewportSize()const {
            return viewportSize;
        }

        void Update(F32 dt) override;
        void Render() override;

    protected:
        void SetupPasses(RenderGraph& renderGraph) override;

        virtual U32x2 GetInternalResolution()const {
            return viewportSize;
        }

    private:
        U32x2 viewportSize = U32x2(0);
        EditorApp& editor;
        SceneView& sceneView;
        F32 outlineTimer = 0.0f;
    };

    class VULKAN_EDITOR_API SceneView : public EditorWidget
    {
    public:
        explicit SceneView(EditorApp& app_);
        virtual ~SceneView();

        void Init();
        void Update(F32 dt);
        void OnGUI() override;
        void LateUpdate()override;
        void Render() override;
        const char* GetName();

        I32x2 GetScreenSize()const {
            return screenSize;
        }

        EditorRenderer& GetRenderer() {
            return *editorRenderer;
        }

        struct EditorIcons* GetEditorIcons();

    private:
        void OnSceneGUI();
        void OnToolbarGUI();
        void HandleEvents();
        void CaptureMouse(bool capture);
        I32x2 GetLocalMousePoint()const;
        void HandleDrop(const AssetItem* item, float x, float y);
        void Manipulate();
        void OnSceneEditing(Scene* newScene) override;

    private:
        EditorApp& app;
        struct WorldViewImpl* worldView = nullptr;
        I32x2 screenPos;
        I32x2 screenSize;
        bool isMouseCaptured = false;
        I32x2 capturedMousePos = I32x2(0, 0);
        F32 cameraSpeed = 0.5f;
        UniquePtr<EditorRenderer> editorRenderer;
        bool shouldRender = false;

        Utils::Action moveForwardAction;
        Utils::Action moveBackAction;
        Utils::Action moveLeftAction;
        Utils::Action moveRightAction;
    };
}
} 
