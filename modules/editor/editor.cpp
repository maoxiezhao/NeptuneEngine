#include "editor.h"
#include "core\utils\profiler.h"
#include "renderer\renderer.h"
#include "renderer\renderPath3D.h"
#include "editor\renderer\imguiRenderer.h"

#include "imgui-docking\imgui.h"
#include "renderer\imguiRenderer.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorRenderer : public RenderPath3D
    {
        void SetupPasses(RenderGraph& renderGraph)
        {
        }

        void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd)
        {
            //RenderPath3D::Compose(renderGraph, cmd);
            ImGuiRenderer::Render(cmd);
        }
    };

    class EditorAppImpl final : public EditorApp
    {
    public:
        EditorAppImpl()
        {

        }

        ~EditorAppImpl()
        {
            // Clear widgets
            for(EditorWidget* plugin : widgets)
                CJING_SAFE_DELETE(plugin);
            widgets.clear();

            // Clear editor plugins
            for(EditorPlugin* plugin : plugins)
                CJING_SAFE_DELETE(plugin);
            plugins.clear();
        }

        void Initialize() override
        {
            App::Initialize();

            ImGuiRenderer::Initialize(*this);
            renderer->ActivePath(&editorRenderer);
        }

        void Uninitialize() override
        {
            ImGuiRenderer::Uninitialize();
            App::Uninitialize();
        }

    protected:
        bool showDemoWindow = true;
        void Update(F32 deltaTime) override
        {
            PROFILE_BLOCK("Update");
            ImGuiRenderer::BeginFrame();

            engine->Update(*world, deltaTime);

            if (showDemoWindow)
                ImGui::ShowDemoWindow(&showDemoWindow);

            ImGuiRenderer::EndFrame();
        }

    public:
        void AddPlugin(EditorPlugin& plugin) override
        {
            plugins.push_back(&plugin);
        }

        void AddWidget(EditorWidget& widget) override
        {
            widgets.push_back(&widget);
        }

        void RemoveWidget(EditorWidget& widget) override
        {
            widgets.swapAndPopItem(&widget);
        }

    private:
        EditorRenderer editorRenderer;
        Array<EditorPlugin*> plugins;
        Array<EditorWidget*> widgets;
    };

    EditorApp* EditorApp::Create()
    {
		return CJING_NEW(EditorAppImpl)();
    }
    
    void EditorApp::Destroy(EditorApp* app)
    {
        CJING_SAFE_DELETE(app);
    }
}
}