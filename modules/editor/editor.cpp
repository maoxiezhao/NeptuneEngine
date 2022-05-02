#include "editor.h"
#include "editorUtils.h"
#include "core\utils\profiler.h"
#include "core\platform\platform.h"
#include "renderer\renderer.h"
#include "renderer\renderPath3D.h"
#include "editor\renderer\imguiRenderer.h"

#include "widgets\assetBrowser.h"
#include "widgets\assetCompiler.h"

#include "imgui-docking\imgui.h"
#include "renderer\imguiRenderer.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorRenderer : public RenderPath3D
    {
        void SetupPasses(RenderGraph& renderGraph) override
        {
        }

        void Compose(RenderGraph& renderGraph, GPU::CommandList* cmd) override
        {
            // RenderPath3D::Compose(renderGraph, cmd);
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

            // Remove actions
            for (Utils::Action* action : actions)
                CJING_SAFE_DELETE(action);
            actions.clear();
        }

        void Initialize() override
        {
            App::Initialize();

            ImGuiRenderer::Initialize(*this);
            renderer->ActivePath(&editorRenderer);

            InitActions();
        }

        void Uninitialize() override
        {
            ImGuiRenderer::Uninitialize();
            App::Uninitialize();
        }

    protected:
        void Update(F32 deltaTime) override
        {
            PROFILE_BLOCK("Update");
            ImGuiRenderer::BeginFrame();

            engine->Update(*world, deltaTime);

            fpsFrame++;
            if (fpsTimer.GetTimeSinceTick() > 1.0f)
            {
                fps = fpsFrame / fpsTimer.Tick();
                fpsFrame = 0;
            }

            OnGUI();

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

        bool showDemoWindow = true;
        void OnGUI()
        {
            ImGuiWindowFlags flags = 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoDocking;

            const bool hasviewports = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable;

            Platform::WindowType window = (Platform::WindowType)platform->GetPlatformWindow();
            Platform::WindowRect rect = Platform::GetClientBounds(window);
            Platform::WindowPoint point = hasviewports ?
                Platform::ToScreen(window, rect.mLeft, rect.mTop) : Platform::WindowPoint();

            U32 winWidth = rect.mRight - rect.mLeft;
            U32 winHeight = rect.mBottom - rect.mTop;
            if (winWidth > 0 && winHeight > 0)
            {
                ImGui::SetNextWindowSize(ImVec2((float)winWidth, (float)winHeight));
                ImGui::SetNextWindowPos(ImVec2((float)point.mPosX, (float)point.mPosY));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::Begin("MainDockspace", nullptr, flags);
                ImGui::PopStyleVar();
                // Show main menu
                OnMainMenu();
                ImGuiID dockspaceID = ImGui::GetID("MyDockspace");
                ImGui::DockSpace(dockspaceID, ImVec2(0, 0));
                ImGui::End();

                // Show editor widgets
                for (auto widget : widgets)
                    widget->OnGUI();

                if (showDemoWindow)
                    ImGui::ShowDemoWindow(&showDemoWindow);
            }
        }

        void OnMainMenu()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 4));
            if (ImGui::BeginMainMenuBar())
            {
                OnFileMenu();
                ImGui::PopStyleVar(2);

                StaticString<128> fpsTxt("");
                fpsTxt << "FPS: ";
                fpsTxt << (U32)(fps + 0.5f);
                auto stats_size = ImGui::CalcTextSize(fpsTxt);
                ImGui::SameLine(ImGui::GetContentRegionMax().x - stats_size.x);
                ImGui::Text("%s", (const char*)fpsTxt);

                ImGui::EndMainMenuBar();
            }
            ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2));
        }

        void OnFileMenu()
        {
            if (!ImGui::BeginMenu("File")) return;

            OnActionMenuItem("Exit");
            ImGui::EndMenu();
        }

        void OnActionMenuItem(const char* name)
        {
            Utils::Action* action = GetAction(name);
            if (!action)
                return;

            if (ImGui::MenuItem(action->label, nullptr, action->isSelected.Invoke()))
                action->func.Invoke();
        }

    private:
        void InitActions()
        {
            // File menu item actions
            AddAction<&EditorAppImpl::Exit>("Exit");
        }

        template<void (EditorAppImpl::*Func)()>
        Utils::Action& AddAction(const char* label)
        {
            Utils::Action* action = CJING_NEW(Utils::Action)(label, label);
            action->func.Bind<Func>(this);
            actions.push_back(action);
            return *action;
        }

        Utils::Action* GetAction(const char* name)
        {
            for (Utils::Action* action : actions)
            {
                if (EqualString(action->name, name))
                    return action;
            }
            return nullptr;
        }

        void Exit()
        {
            RequestShutdown();
        }

    private:
        EditorRenderer editorRenderer;
        Array<EditorPlugin*> plugins;
        Array<EditorWidget*> widgets;
        F32 fps = 0.0f;
        U32 fpsFrame = 0;
        Timer fpsTimer;

        Array<Utils::Action*> actions;
    };

    EditorApp* EditorApp::Create()
    {
        // TODO
		// return CJING_NEW(EditorAppImpl)();
        return new EditorAppImpl();
    }
    
    void EditorApp::Destroy(EditorApp* app)
    {
        // CJING_SAFE_DELETE(app);
    }
}
}