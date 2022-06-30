#include "editor.h"
#include "editorUtils.h"
#include "core\utils\profiler.h"
#include "core\platform\platform.h"
#include "editor\renderer\imguiRenderer.h"
#include "editor\settings.h"

#include "widgets\assetBrowser.h"
#include "widgets\assetCompiler.h"
#include "widgets\worldEditor.h"
#include "widgets\log.h"
#include "widgets\property.h"
#include "widgets\entityList.h"
#include "widgets\renderGraph.h"

#include "plugins\renderer.h"

#include "imgui-docking\imgui.h"
#include "renderer\imguiRenderer.h"
#include "renderer\model.h"

namespace VulkanTest
{
namespace Editor
{
    void EditorRenderer::Render()
    {
        UpdateRenderData();

        GPU::DeviceVulkan* device = wsi->GetDevice();
        auto& graph = GetRenderGraph();
        graph.SetupAttachments(*device, nullptr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // Used for sceneView
        Jobsystem::JobHandle handle;
        graph.Render(*device, handle);
        Jobsystem::Wait(&handle);
    }

    class EditorAppImpl final : public EditorApp
    {
    public:
        EditorAppImpl() :
            settings(*this)
        {
            memset(imguiKeyMap, 0, sizeof(imguiKeyMap));
            imguiKeyMap[(int)Platform::Keycode::CTRL] = ImGuiKey_ModCtrl;
            imguiKeyMap[(int)Platform::Keycode::MENU] = ImGuiKey_ModAlt;
            imguiKeyMap[(int)Platform::Keycode::SHIFT] = ImGuiKey_ModShift;
            imguiKeyMap[(int)Platform::Keycode::LSHIFT] = ImGuiKey_LeftShift;
            imguiKeyMap[(int)Platform::Keycode::RSHIFT] = ImGuiKey_RightShift;
            imguiKeyMap[(int)Platform::Keycode::SPACE] = ImGuiKey_Space;
            imguiKeyMap[(int)Platform::Keycode::TAB] = ImGuiKey_Tab;
            imguiKeyMap[(int)Platform::Keycode::LEFT] = ImGuiKey_LeftArrow;
            imguiKeyMap[(int)Platform::Keycode::RIGHT] = ImGuiKey_RightArrow;
            imguiKeyMap[(int)Platform::Keycode::UP] = ImGuiKey_UpArrow;
            imguiKeyMap[(int)Platform::Keycode::DOWN] = ImGuiKey_DownArrow;
            imguiKeyMap[(int)Platform::Keycode::PAGEUP] = ImGuiKey_PageUp;
            imguiKeyMap[(int)Platform::Keycode::PAGEDOWN] = ImGuiKey_PageDown;
            imguiKeyMap[(int)Platform::Keycode::HOME] = ImGuiKey_Home;
            imguiKeyMap[(int)Platform::Keycode::END] = ImGuiKey_End;
            imguiKeyMap[(int)Platform::Keycode::DEL] = ImGuiKey_Delete;
            imguiKeyMap[(int)Platform::Keycode::BACKSPACE] = ImGuiKey_Backspace;
            imguiKeyMap[(int)Platform::Keycode::RETURN] = ImGuiKey_Enter;
            imguiKeyMap[(int)Platform::Keycode::ESCAPE] = ImGuiKey_Escape;
            imguiKeyMap[(int)Platform::Keycode::A] = ImGuiKey_A;
            imguiKeyMap[(int)Platform::Keycode::C] = ImGuiKey_C;
            imguiKeyMap[(int)Platform::Keycode::V] = ImGuiKey_V;
            imguiKeyMap[(int)Platform::Keycode::X] = ImGuiKey_X;
            imguiKeyMap[(int)Platform::Keycode::Y] = ImGuiKey_Y;
            imguiKeyMap[(int)Platform::Keycode::Z] = ImGuiKey_Z;
        }

        ~EditorAppImpl()
        {
        }

        virtual U32 GetDefaultWidth() {
            return 1600;
        }

        virtual U32 GetDefaultHeight() {
            return 900;
        }

        virtual const char* GetWindowTitle() {
            return "VulkanEditor";
        }

        void Initialize() override
        {
            // Init platform
            bool ret = platform->Init(GetDefaultWidth(), GetDefaultHeight(), GetWindowTitle());
            ASSERT(ret);

            // Init wsi
            ret = wsi.Initialize(Platform::GetCPUsCount() + 1);
            ASSERT(ret);

            // Add main window
            mainWindow = platform->GetWindow();
            windows.push_back(mainWindow);

            // Create game engine
            Engine::InitConfig config = {};
            config.windowTitle = GetWindowTitle();
            engine = CreateEngine(config, *this);

            assetCompiler = AssetCompiler::Create(*this);
            worldEditor = WorldEditor::Create(*this);
            assetBrowser = AssetBrowser::Create(*this);
            renderGraphWidget = RenderGraphWidget::Create(*this);
            propertyWidget = CJING_MAKE_UNIQUE<PropertyWidget>(*this);
            entityListWidget = CJING_MAKE_UNIQUE<EntityListWidget>(*this);
            logWidget = CJING_MAKE_UNIQUE<LogWidget>();

            // Load editor settings
            LoadSettings();

            // Get renderer
            renderer = static_cast<RendererPlugin*>(engine->GetPluginManager().GetPlugin("Renderer"));
            ASSERT(renderer != nullptr);
            editorRenderer = CJING_MAKE_UNIQUE<EditorRenderer>();
            editorRenderer->DisableSwapchain();
            renderer->ActivePath(editorRenderer.Get());

            // Init imgui renderer
            ImGuiRenderer::Initialize(*this, settings);

            // Init actions
            InitActions();

            // Load plugins
            LoadPlugins();

            AddWidget(*assetBrowser);
            AddWidget(*entityListWidget);
            AddWidget(*renderGraphWidget);
            AddWidget(*propertyWidget);
            AddWidget(*logWidget);

            assetCompiler->InitFinished();
            assetBrowser->InitFinished();
        }

        void Uninitialize() override
        {
            SaveSettings();

            FileSystem& fs = engine->GetFileSystem();
            while (fs.HasWork())
                fs.ProcessAsync();

            // Destroy world
            worldEditor->DestroyWorld();

            // Clear widgets
            widgets.clear();

            // Clear editor plugins
            for (EditorPlugin* plugin : plugins)
                CJING_SAFE_DELETE(plugin);
            plugins.clear();

            // Remove actions
            for (Utils::Action* action : actions)
                CJING_SAFE_DELETE(action);
            actions.clear();

            // Remove system widgets
            assetBrowser.Reset();
            assetCompiler.Reset();
            worldEditor.Reset();
            entityListWidget.Reset();
            renderGraphWidget.Reset();
            propertyWidget.Reset();
            logWidget.Reset();

            // Uninit imgui renderer
            ImGuiRenderer::Uninitialize();

            // Reset engine
            editorRenderer.Reset();
            engine.Reset();

            // Uninit platform
            wsi.Uninitialize();
            platform.reset();
        }

        void OnEvent(const Platform::WindowEvent& ent) override
        {
            bool isFocused = IsFocused();
            switch (ent.type)
            {
            case Platform::WindowEvent::Type::MOUSE_MOVE: {
                ImGuiIO& io = ImGui::GetIO();
                const Platform::WindowPoint cp = Platform::GetMouseScreenPos();
                if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                    io.AddMousePosEvent((float)cp.x, (float)cp.y);
                }
                else {
                    const Platform::WindowRect screenRect = Platform::GetWindowScreenRect(mainWindow);
                    io.AddMousePosEvent((float)cp.x - screenRect.left, (float)cp.y - screenRect.top);
                }
                break;
            }
            case Platform::WindowEvent::Type::FOCUS: {
                ImGuiIO& io = ImGui::GetIO();
                io.AddFocusEvent(IsFocused());
                break;
            }
            case Platform::WindowEvent::Type::MOUSE_BUTTON: {
                ImGuiIO& io = ImGui::GetIO();
                if (isFocused || !ent.mouseButton.down)
                    io.AddMouseButtonEvent((int)ent.mouseButton.button, ent.mouseButton.down);
                break;
            }
            case Platform::WindowEvent::Type::MOUSE_WHEEL:
                if (isFocused)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    io.AddMouseWheelEvent(0, ent.mouseWheel.amount);
                }
                break;

            case Platform::WindowEvent::Type::CHAR:
                if (isFocused)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    char tmp[5] = {};
                    memcpy(tmp, &ent.textInput.utf8, sizeof(ent.textInput.utf8));
                    io.AddInputCharactersUTF8(tmp);
                }
                break;

            case Platform::WindowEvent::Type::KEY:
                if (isFocused || !ent.key.down)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    ImGuiKey key = imguiKeyMap[(int)ent.key.keycode];
                    if (key != ImGuiKey_None)
                        io.AddKeyEvent(key, ent.key.down);
                }
                break;

            case Platform::WindowEvent::Type::QUIT:
                RequestShutdown();
                break;

            case Platform::WindowEvent::Type::WINDOW_CLOSE: {
                ImGuiViewport* vp = ImGui::FindViewportByPlatformHandle(ent.window);
                if (vp)
                    vp->PlatformRequestClose = true;
                if (ent.window == mainWindow)
                    RequestShutdown();
                break;
            }
            case Platform::WindowEvent::Type::WINDOW_MOVE: {
                if (ImGui::GetCurrentContext())
                {
                    ImGuiViewport* vp = ImGui::FindViewportByPlatformHandle(ent.window);
                    if (vp)
                        vp->PlatformRequestMove = true;
                }
                if (ent.window == mainWindow)
                {
                    settings.window.x = ent.winMove.x;
                    settings.window.y = ent.winMove.y;
                }
                break;
            }
            case Platform::WindowEvent::Type::WINDOW_SIZE:
                if (ImGui::GetCurrentContext()) 
                {
                    ImGuiViewport* vp = ImGui::FindViewportByPlatformHandle(ent.window);
                    if (vp)
                        vp->PlatformRequestResize = true;
                }

                if (ent.window == mainWindow && ent.winSize.w > 0 && ent.winSize.h > 0)
                {
                    settings.window.w = ent.winSize.w;
                    settings.window.h = ent.winSize.h;
                    platform->NotifyResize(ent.winSize.w, ent.winSize.h);
                }

                break;

            default:
                break;
            }
        }

        EditorRenderer& GetEditorRenderer()
        {
            return *editorRenderer;
        }

        AssetCompiler& GetAssetCompiler()
        {
            return *assetCompiler;
        }

        EntityListWidget& GetEntityList()
        {
            return *entityListWidget;
        }

    protected:
        void Update(F32 deltaTime) override
        {
            PROFILE_BLOCK("Update");
            ProcessDeferredDestroyWindows();
           
            // Begin imgui frame
            ImGuiRenderer::BeginFrame();

            assetCompiler->Update(deltaTime);

            engine->Update(*worldEditor->GetWorld(), deltaTime);

            fpsFrame++;
            if (fpsTimer.GetTimeSinceTick() > 1.0f)
            {
                fps = fpsFrame / fpsTimer.Tick();
                fpsFrame = 0;
            }

            // Update editor widgets
            for (auto widget : widgets)
                widget->Update(deltaTime);

            // Update gui
            OnGUI();

            // Test
            // Renderer::UpdateGeometryBuffer();

            // End imgui frame
            ImGuiRenderer::EndFrame();
        }

        struct ShaderGeometry
        {
            I32 vbPos;
            I32 vbNor;
            I32 vbUVs;
            I32 ib;
        };

        void Render() override
        {
            wsi.BeginFrame();

            for (auto widget : widgets)
                widget->Render();

            ImGuiRenderer::Render();
            wsi.EndFrame();
            wsi.GetDevice()->MoveReadWriteCachesToReadOnly();
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

        void AddWindow(Platform::WindowType window) override
        {
            windows.push_back(window);
        }
        void RemoveWindow(Platform::WindowType window) override
        {
            windows.erase(window);
        }

        void DeferredDestroyWindow(Platform::WindowType window)override
        {
            toDestroyWindows.push_back({ window, 4 });
        }

        void LoadSettings()
        {
            Logger::Info("Loading settings...");
            settings.Load();

            if (settings.window.w > 0 && settings.window.h > 0)
            {
                Platform::WindowRect rect = Platform::GetWindowScreenRect(mainWindow);
                rect.left = settings.window.x;
                rect.top = settings.window.y;
                //rect.width = settings.window.w;
                //rect.height = settings.window.h;
                Platform::SetWindowScreenRect(mainWindow, rect);
            }
        }

        void SaveSettings() override
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantSaveIniSettings) 
            {
                const char* data = ImGui::SaveIniSettingsToMemory();
                settings.imguiState = data;
            }

            settings.Save();
        }

        bool showDemoWindow = false;
        void OnGUI()
        {
            ImGuiWindowFlags flags = 
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                ImGuiWindowFlags_NoDocking;

            const bool hasviewports = ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable;

            Platform::WindowRect rect = Platform::GetClientBounds(mainWindow);
            Platform::WindowPoint point = hasviewports ? Platform::ToScreen(mainWindow, rect.left, rect.top) : Platform::WindowPoint();
            if (rect.width > 0 && rect.height > 0)
            {
                ImGui::SetNextWindowSize(ImVec2((float)rect.width, (float)rect.height));
                ImGui::SetNextWindowPos(ImVec2((float)point.x, (float)point.y));
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

                auto rect = Platform::GetClientBounds(mainWindow);

                StaticString<128> fpsTxt("");
                fpsTxt << (U32)rect.width << "x" << (U32)rect.height;
                fpsTxt << " FPS: ";
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
        bool IsFocused() const 
        {
            const Platform::WindowType focused = Platform::GetActiveWindow();
            const int idx = windows.find([focused](Platform::WindowType w) { return w == focused; });
            return idx >= 0;
        }

        void ProcessDeferredDestroyWindows()
        {
            for (int i = toDestroyWindows.size() - 1; i >= 0; i--)
            {
                WindowToDestroy& toDestroy = toDestroyWindows[i];
                toDestroy.counter--;
                if (toDestroy.counter == 0)
                {
                    Platform::DestroyCustomWindow(toDestroy.window);
                    toDestroyWindows.swapAndPop(i);
                }
            }
        }

        void InitActions()
        {
            // File menu item actions
            AddAction<&EditorAppImpl::Exit>("Exit");
        }

        void LoadPlugins()
        {
            // TODO:
            {
                EditorPlugin* plugin = SetupPluginRenderer(*this);
                if (plugin != nullptr)
                    AddPlugin(*plugin);
            }

            // Init plugins
            for (EditorPlugin* plugin : plugins)
                plugin->Initialize();
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
        Array<EditorPlugin*> plugins;
        Array<EditorWidget*> widgets;
        F32 fps = 0.0f;
        U32 fpsFrame = 0;
        Timer fpsTimer;
        Array<Utils::Action*> actions;
        Settings settings;
        UniquePtr<EditorRenderer> editorRenderer;

        // Windows
        Platform::WindowType mainWindow;
        Array<Platform::WindowType> windows;
        struct WindowToDestroy 
        {
            Platform::WindowType window;
            U32 counter = 0;
        };
        Array<WindowToDestroy> toDestroyWindows;

        // Builtin widgets
        UniquePtr<AssetCompiler> assetCompiler;
        UniquePtr<AssetBrowser> assetBrowser;
        UniquePtr<WorldEditor> worldEditor;
        UniquePtr<LogWidget> logWidget;
        UniquePtr<PropertyWidget> propertyWidget;
        UniquePtr<EntityListWidget> entityListWidget;
        UniquePtr<RenderGraphWidget> renderGraphWidget;

        // Imgui
        ImGuiKey imguiKeyMap[255];
    };

    EditorApp* EditorApp::Create()
    {
		return CJING_NEW(EditorAppImpl)();
    }
    
    void EditorApp::Destroy(EditorApp* app)
    {
        // CJING_SAFE_DELETE(app);
    }
}
}