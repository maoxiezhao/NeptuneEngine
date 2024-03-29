#include "editor.h"
#include "editorUtils.h"
#include "projectInfo.h"
#include "core\commandLine.h"
#include "core\platform\platform.h"
#include "core\scene\reflection.h"
#include "core\utils\string.h"
#include "editor\renderer\imguiRenderer.h"
#include "editor\settings.h"
#include "editor\profiler\profilerTools.h"
#include "editor\states\editorStateMachine.h"
#include "renderer\imguiRenderer.h"
#include "renderer\imageUtil.h"
#include "renderer\textureHelper.h"
#include "renderer\render2D\fontResource.h"
#include "renderer\render2D\font.h"

#include "modules\contentDatabase.h"
#include "modules\thumbnails.h"
#include "modules\level.h"
#include "modules\renderer.h"
#include "modules\sceneEditing.h"

#include "widgets\splashScreen.h"
#include "widgets\assetBrowser.h"
#include "widgets\assetImporter.h"
#include "widgets\log.h"
#include "widgets\property.h"
#include "widgets\entityList.h"
#include "widgets\renderGraph.h"
#include "widgets\gizmo.h"
#include "widgets\profiler.h"

#include "imgui-docking\imgui.h"

namespace VulkanTest
{
namespace Editor
{
    class EditorAppImpl final : public EditorApp
    {
    public:
        EditorAppImpl() :
            settings(*this),
            profilerTools(*this),
            stateMachine(*this)
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
            CloseSplashScreen();
        }

        virtual U32 GetDefaultWidth() {
            return 1600;
        }

        virtual U32 GetDefaultHeight() {
            return 900;
        }

        virtual const char* GetWindowTitle() {
            return Globals::ProductName;
        }

        void CloseSplashScreen()
        {
            CJING_SAFE_DELETE(splashScreen);
            splashScreen = nullptr;
        }

        bool InitializeProject()
        {
            Globals::ProductName = "Editor";

#ifdef CJING3D_TESTS
            if (CommandLine::options.projectPath.empty())
            {
                char projectPath[MAX_PATH_LENGTH];
                Platform::GetSpecialFolderPath(Platform::SpecialFolder::Temporary, projectPath);
                CommandLine::options.projectPath = projectPath;
            }
            CommandLine::options.newProject = false;
            Path projectPath = Path("C:/Github/Vulkan-Test/workspace");
#else
            Path projectPath = Path(CommandLine::options.projectPath);
#endif
            if (CommandLine::options.newProject)
            {
                if (projectPath.IsEmpty())
                {
                    char workingPath[MAX_PATH_LENGTH];
                    Platform::GetCurrentDir(workingPath);
                    projectPath = workingPath;
                }
                else if (!Platform::DirExists(projectPath.c_str()))
                    Platform::MakeDir(projectPath.c_str());

                char tmp[MAX_PATH_LENGTH];
                memcpy(tmp, projectPath.c_str(), projectPath.Length());
                tmp[projectPath.Length() - 1] = '\0';
                PathInfo pathInfo(tmp);

                // Create project
                ProjectInfo project;
                project.Name = pathInfo.basename;
                project.ProjectPath = StaticString<MAX_PATH_LENGTH>((projectPath / project.Name).c_str(), ".proj");
                project.ProjectFolderPath = projectPath;

                // Project always link to the engine
                auto& ref = project.References.emplace();
                ref.Name = "$(EnginePath)/engine.proj";
                if (!project.Save())
                {
                    Platform::Fatal("Failed to save project");
                    return false;
                }

                if (!Platform::DirExists(projectPath / "content"))
                    Platform::MakeDir(projectPath / "content");
            }

            if (projectPath.IsEmpty())
            {
                Platform::Fatal("Missing project");
                return false;
            }

            if (!Platform::DirExists(projectPath.c_str()))
            {
                Platform::Fatal("Project folder is missing");
                return false;
            }

            // Init global paths
            Globals::ProjectFolder = projectPath;
            char startupPath[MAX_PATH_LENGTH];
#ifdef CJING3D_EDITOR
            if (!CommandLine::options.workingPath.empty())
                memcpy(startupPath, CommandLine::options.workingPath.c_str(), CommandLine::options.workingPath.size());
            else
                Platform::GetCurrentDir(startupPath);
#else
            Platform::GetCurrentDir(startupPath);
#endif
            Engine::InitGlobalPaths(startupPath);

            // Load project
            Array<Path> projectFiles;
            auto fileList = FileSystem::Enumerate(projectPath);
            for (const auto& fileInfo : fileList)
            {
                if (fileInfo.type == PathType::Directory)
                    continue;

                if (EndsWith(fileInfo.filename, ".proj"))
                    projectFiles.push_back(projectPath / fileInfo.filename);   
            }

            if (projectFiles.size() == 0)
            {
                Platform::Fatal("Missing project file");
                return false;
            }
            else if (projectFiles.size() > 1)
            {
                Platform::Fatal("Too many project file");
                return false;
            }

            auto project = CJING_NEW(ProjectInfo);
            if (!project->Load(Path(projectFiles[0])))
            {
                Platform::Fatal("Failed to load project.");
                return false;
            }
            ProjectInfo::EditorProject = project;

            auto engineProjectPath = Globals::StartupFolder / "engine.proj";
            project = ProjectInfo::LoadProject(engineProjectPath);
            if (!project)
            {
                Platform::Fatal("Failed to load engine project.");
                return false;
            }
            ProjectInfo::EngineProject = project;

            return true;
        }

        void Initialize() override
        {
            Profiler::Enable(true);

            // Init project
            if (!InitializeProject())
                return;

            // Init platform
            bool ret = platform->Init(GetDefaultWidth(), GetDefaultHeight(), GetWindowTitle(), false);
            ASSERT(ret);

            // Add main window
            mainWindow = platform->GetWindow();
            windows.push_back(mainWindow);

            // Create game engine
            engine = CreateEngine(*this);
            Engine::Instance = engine.Get();

            // Show splash screen
            splashScreen = nullptr;
            splashScreen = CJING_NEW(SplashScreen);
            splashScreen->Show(GetWSI());

            // Init asset importer first
            assetImporter = AssetImporter::Create(*this);

            // Load plugins
            engine->LoadPlugins();

            // Init widgets
            assetBrowser = AssetBrowser::Create(*this);
            renderGraphWidget = RenderGraphWidget::Create(*this);
            entityListWidget = CJING_MAKE_UNIQUE<EntityListWidget>(*this);
            propertyWidget = CJING_MAKE_UNIQUE<PropertyWidget>(*this);
            profilerWidget = ProfilerWidget::Create(*this);
            logWidget = CJING_MAKE_UNIQUE<LogWidget>();

            // Load modules
            RegisterModule(contentDatabase = CJING_NEW(ContentDatabaseModule)(*this));
            RegisterModule(thumbnails = ThumbnailsModule::Create(*this));
            RegisterModule(rendererModule = RendererModule::Create(*this));
            RegisterModule(levelModule = LevelModule::Create(*this));
            RegisterModule(sceneEditingModule = SceneEditingModule::Create(*this));

            for (auto editorModule : editorModules)
                editorModule->Initialize();

            // Create imgui context
            ImGuiRenderer::CreateContext();

            // Load editor settings
            LoadSettings();

            // Get renderer
            renderer = static_cast<RendererPlugin*>(engine->GetPluginManager().GetPlugin("Renderer"));
            ASSERT(renderer != nullptr);

            // Init imgui renderer
            ImGuiRenderer::Initialize(*this, settings);

            // Load fonts
            LoadFonts();

            // Init actions
            InitActions();

            // Load plugins
            LoadPlugins();

            AddWidget(*assetBrowser);
            AddWidget(*entityListWidget);
            AddWidget(*profilerWidget);
            AddWidget(*renderGraphWidget);
            AddWidget(*propertyWidget);
            AddWidget(*logWidget);

            // Load settings again for actions
            LoadSettings();

            // Init reflection
            InitReflection();

            // Notify initFinished
            stateMachine.GetLoadingState()->InitFinished();
        }

        void Uninitialize() override
        {
            // Save editor settings
            SaveSettings();

            // Clear world
            ClearWorld();

            // Clear widgets
            widgets.clear();

            // Clear modules
            for (auto editorModule : editorModules)
            {
                editorModule->Unintialize();
                CJING_SAFE_DELETE(editorModule);
            }
            editorModules.clear();

            // Clear project
            CJING_SAFE_DELETE(ProjectInfo::EditorProject);
            ProjectInfo::EditorProject = nullptr;
            for (auto project : ProjectInfo::ProjectsCache) {
                CJING_DELETE(project);
            }
            ProjectInfo::ProjectsCache.release();

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
            assetImporter.Reset();
            entityListWidget.Reset();
            profilerWidget.Reset();
            renderGraphWidget.Reset();
            propertyWidget.Reset();
            logWidget.Reset();

            // Uninit imgui renderer
            ImGuiRenderer::Uninitialize();

            // Dispose import file entries
            ImportFileEntry::Dispose();

            // Reset engine
            engine.Reset();
            Engine::Instance = nullptr;

            // Uninit platform
            platform.reset();
        }

        void OnEvent(const Platform::WindowEvent& ent) override
        {
            windowEvents.push_back(ent);

            bool isFocused = IsFocused();
            switch (ent.type)
            {
            case Platform::WindowEvent::Type::MOUSE_MOVE: {
                if (!mouseCaptured)
                {
                    ImGuiIO& io = ImGui::GetIO();
                    const Platform::WindowPoint cp = Platform::GetMouseScreenPos();
                    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
                        io.AddMousePosEvent((float)cp.x, (float)cp.y);
                    }
                    else {
                        const Platform::WindowRect screenRect = Platform::GetWindowScreenRect(mainWindow);
                        io.AddMousePosEvent((float)cp.x - screenRect.left, (float)cp.y - screenRect.top);
                    }
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
                Engine::RequestExit();
                break;

            case Platform::WindowEvent::Type::WINDOW_CLOSE: {
                ImGuiViewport* vp = ImGui::FindViewportByPlatformHandle(ent.window);
                if (vp)
                    vp->PlatformRequestClose = true;
                if (ent.window == mainWindow)
                    Engine::RequestExit();
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

        AssetBrowser& GetAssetBrowser() override
        {
            return *assetBrowser;
        }

        AssetImporter& GetAssetImporter() override
        {
            return *assetImporter;
        }

        EntityListWidget& GetEntityList() override
        {
            return *entityListWidget;
        }

        EditorStateMachine& GetStateMachine() override
        {
            return stateMachine;
        }

        Gizmo::Config& GetGizmoConfig() override
        {
            return gizmoConfig;
        }

        ImFont* GetBigIconFont() override 
        { 
            return bigIconFont;
        }

        ImFont* GetBoldFont() override 
        { 
            return boldFont;
        }

        const AddComponentTreeNode* GetAddComponentTreeNodeRoot()const override
        {
            return &addCompTreeNodeRoot;
        }

        ProfilerTools& GetProfilerTools() override
        {
            return profilerTools;
        }

        Platform::WindowType GetMainWindow() override
        {
            return mainWindow;
        }

        void RegisterModule(EditorModule* module) override
        {
            editorModules.push_back(module);
        }

        ContentDatabaseModule& GetContentDatabaseModule() override
        {
            return *contentDatabase;
        }

        ThumbnailsModule& GetThumbnailsModule() override
        {
            return *thumbnails;
        }

        RendererModule& GetRendererModule() override
        {
            return *rendererModule;
        }

        LevelModule& GetLevelModule() override
        {
            return *levelModule;
        }

        SceneEditingModule& GetSceneEditingModule() override
        {
            return *sceneEditingModule;
        }

        const char* GetComponentIcon(ComponentType compType) const override
        {
            auto it = componentIcons.find(compType.index);
            return it.isValid() ? it.value().c_str() : "";
        }

        const char* GetComponentTypeName(ComponentType compType) const override
        {
            auto it = componentLabels.find(compType.index);
            return it.isValid() ? it.value().c_str() : "Unknown";
        }

    protected:
        void Update(F32 deltaTime) override
        {
            PROFILE_BLOCK("Update");
            ProcessDeferredDestroyWindows();
           
            // Begin imgui frame
            ImGuiRenderer::BeginFrame();
            ImGui::PushFont(font);

            assetImporter->Update(deltaTime);
            Gizmo::Update();

            engine->Update(sceneEditingModule->GetEditingWorld(), deltaTime);

            // Update editor modules
            for (auto editorModule : editorModules)
                editorModule->Update();

            // Update editor widgets
            for (auto widget : widgets)
                widget->Update(deltaTime);

            // Update gui
            OnGUI();

            // LateUpdate editor widgets
            for (auto widget : widgets)
                widget->LateUpdate();

            // End imgui frame
            ImGui::PopFont();
            ImGuiRenderer::EndFrame();

            windowEvents.clear();
        }

        void FrameEnd() override
        {
            // Modules frame end
            for (auto module : editorModules)
                module->EndFrame();

            // Widgets frame end
            for (auto widget : widgets)
                widget->EndFrame();

            // Update profiler
            profilerTools.Update();
        }

        void Render() override
        {
            PROFILE_BLOCK("Render");

            F32 beforeDrawTime = timer.GetTimeSinceTick();
            auto& wsi = GetWSI();

            // Show splash screen
            if (splashScreen)
            {
                splashScreen->Render();
            }
            else
            {
                for (auto widget : widgets)
                    widget->Render();

                // Draw debug gizmo for selected components
                ShowComponentGizmo();

                ImGuiRenderer::Render();
            }
     
            drawTime = timer.GetTimeSinceTick() - beforeDrawTime;
            wsi.GetDevice()->MoveReadWriteCachesToReadOnly();

            // Calculate FPS
            fpsFrames++;
            if (fpsTimer.GetTimeSinceTick() > 1.0f)
            {
                fps = fpsFrames / fpsTimer.Tick();
                fpsFrames = 0;
            }
        }

    public:
        void InitFinished()override
        {
            Logger::Info("Editor end initialize");
            stateMachine.SwitchState(EditorStateType::EditingScene);

            // Asset importer process pending tasks
            assetImporter->InitFinished();

            // Modules
            for (auto editorModule : editorModules)
                editorModule->InitFinished();

            // Init all widgets
            for (auto widget : widgets)
                widget->InitFinished();

            CloseSplashScreen();
            mainWindow = platform->GetWindow();
            ASSERT(mainWindow);
            Platform::ShowCustomWindow(mainWindow);

            // Loading scene if necessary
            // TODO
        }

        void AddPlugin(EditorPlugin& plugin) override
        {
            plugins.push_back(&plugin);
        }

        void AddWidget(EditorWidget& widget) override
        {
            widgets.push_back(&widget);
        }

        EditorWidget* GetWidget(const char* name) override
        {
            for (auto widget : widgets)
            {
                if (EqualString(widget->GetName(), name))
                    return widget;
            }
            return nullptr;
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
                Platform::SetWindowScreenRect(mainWindow, rect);
            }
        }

        void LoadFonts()
        {
            ImGuiIO& io = ImGui::GetIO();

            // Add fonts
            const I32 dpi = Platform::GetDPI();
            F32 fontScale = dpi / 96.0f;
            F32 fontSize = settings.fontSize * fontScale;

            font = AddFontFromFile("editor/fonts/notosans-regular.ttf", fontSize, true);
            boldFont = AddFontFromFile("editor/fonts/notosans-bold.ttf", fontSize, true);

            // Load big icon font
            OutputMemoryStream iconsData;
            if (FileSystem::LoadContext("editor/fonts/fa-solid-900.ttf", iconsData))
            {
                F32 iconSize = fontSize * 1.25f;

                ImFontConfig cfg;
                CopyString(cfg.Name, "editor/fonts/fa-solid-900.ttf");
                cfg.FontDataOwnedByAtlas = false;
                cfg.GlyphMinAdvanceX = iconSize;
                static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
                bigIconFont = io.Fonts->AddFontFromMemoryTTF((void*)iconsData.Data(), (I32)iconsData.Size(), iconSize, &cfg, iconRanges);
#if 0
                cfg.MergeMode = true;
                CopyString(cfg.Name, "editor/fonts/fa-regular-400.ttf");
                iconsData.Clear();
                if (FileSystem::LoadContext(cfg.Name, iconsData))
                {
                    ImFont* iconsFont = io.Fonts->AddFontFromMemoryTTF((void*)iconsData.Data(), (I32)iconsData.Size(), iconSize, &cfg, iconRanges);
                    ASSERT(iconsFont);
                }
#endif
            }
        }

        void AddAction(Utils::Action* action)
        {
            actions.push_back(action);
        }

        void RemoveAction(Utils::Action* action)
        {
            actions.erase(action);
        }

        Array<Utils::Action*>& GetActions()
        {
            return actions;
        }

        void SetCursorCaptured(bool capture)
        {
            mouseCaptured = capture;
        }

        void SaveSettings() override
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantSaveIniSettings) 
            {
                const char* data = ImGui::SaveIniSettingsToMemory();
                settings.imguiState = data;
            }

            Platform::WindowRect rect = Platform::GetWindowScreenRect(mainWindow);
            settings.window.x = rect.left;
            settings.window.y = rect.top;
            settings.Save();
        }

        void SetRenderInterace(RenderInterface* renderInterface_) override
        {
            renderInterface = renderInterface_;
        }

        const Array<Platform::WindowEvent>& GetWindowEvents()const override
        {
            return windowEvents;
        }

        RenderInterface* GetRenderInterface()override
        {
            return renderInterface;
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

                assetImporter->OnGUI();
                settings.OnGUI();
                OnAboundGUI();

                if (showDemoWindow)
                    ImGui::ShowDemoWindow(&showDemoWindow);

                auto state = stateMachine.GetCurrentStateType();
                if (state == EditorStateType::ChangingScene)
                {
                    const F32 uiWidth = std::max(300.f, ImGui::GetIO().DisplaySize.x * 0.33f);
                    const ImVec2 pos = ImGui::GetMainViewport()->Pos;
                    ImGui::SetNextWindowPos(ImVec2((ImGui::GetIO().DisplaySize.x - uiWidth) * 0.5f + pos.x, ImGui::GetIO().DisplaySize.y * 0.4f + pos.y));
                    ImGui::SetNextWindowSize(ImVec2(uiWidth, -1));
                    ImGui::SetNextWindowSizeConstraints(ImVec2(-FLT_MAX, 0), ImVec2(FLT_MAX, 200));
                    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoFocusOnAppearing
                        | ImGuiWindowFlags_NoInputs
                        | ImGuiWindowFlags_NoNav
                        | ImGuiWindowFlags_AlwaysAutoResize
                        | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoSavedSettings;
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1);

                    if (ImGui::Begin("SceneStateNotify", nullptr, flags))
                    {
                        ImGui::Text("%s", "Changing scenes...");
                        static U32 totalCount = 100;
                        static U32 currentCount = 0;
                        ImGui::ProgressBar(((F32)currentCount) / (F32)totalCount);
                        currentCount = (currentCount + 1) % totalCount;
                    }
                    ImGui::End();
                    ImGui::PopStyleVar();
                }
            }
        }

        void OnMainMenu()
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 4));
            if (ImGui::BeginMainMenuBar())
            {
                OnFileMenu();
                OnEditMenu();
                OnEntityMenu();
                OnViewMenu();
                OnHelpMenu();
                ImGui::PopStyleVar(2);

                float w = (ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x) * 0.5f - 30 - ImGui::GetCursorPosX();
                ImGui::Dummy(ImVec2(w, ImGui::GetTextLineHeightWithSpacing()));
                GetAction("SaveEditingSecne")->ToolbarButton(bigIconFont);
                GetAction("SaveAllSecnes")->ToolbarButton(bigIconFont);
                GetAction("ToggleGameMode")->ToolbarButton(bigIconFont);

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

        void OnEditMenu()
        {
            if (!ImGui::BeginMenu("Edit")) return;
            ImGui::EndMenu();
        }

        void OnEntityMenu()
        {
            bool enable = sceneEditingModule->GetEditingScene() != nullptr;
            if (!enable)
                ImGui::BeginDisabled();

            if (ImGui::BeginMenu("Entity"))
            {
                auto& folders = sceneEditingModule->GetEditingScene()->GetFolders();
                entityListWidget->ShowCreateEntityGUI(folders.GetSelectedFolder());
                ImGui::EndMenu();
            }

            if (!enable)
                ImGui::EndDisabled();
        }

        void OnViewMenu()
        {
            if (!ImGui::BeginMenu("View")) return;
            ImGui::MenuItem(ICON_FA_IMAGES "Asset browser", nullptr, &assetBrowser->isOpen);
            ImGui::MenuItem(ICON_FA_COMMENT_ALT "Log", nullptr, &logWidget->isOpen);
            ImGui::MenuItem(ICON_FA_STREAM "EntityList", nullptr, &entityListWidget->isOpen);
            ImGui::MenuItem(ICON_FA_CHART_AREA "Profiler", nullptr, &profilerWidget->isOpen);
            ImGui::MenuItem(ICON_FA_STREAM "EditorSetting", nullptr, &settings.isOpen);
            ImGui::EndMenu();
        }

        bool showAbout = false;
        void OnHelpMenu()
        {
            if (!ImGui::BeginMenu("Help")) return;
            ImGui::MenuItem(ICON_FA_USER "About", nullptr, &showAbout);
            ImGui::EndMenu();
        }

        void OnAboundGUI()
        {
            if (showAbout == false)
                return;

            if (ImGui::Begin(ICON_FA_USER "About", &showAbout))
            {
                ImGuiEx::Rect(72, 72, 0xffffFFFF);
                ImGui::SameLine();
        
                ImGui::BeginGroup();
                {
                    ImGui::PushFont(boldFont);
                    ImGui::Text("VulkanTest (Temporary name)");
                    ImGui::PopFont();
                    ImGui::Text("Version:");
                    ImGui::SameLine();
                    ImGui::Text(CJING_VERSION_TEXT);
                    ImGui::Text("Copyright (c) 2021-2022 ZYYYYY. All rights reserved.");
                    ImGui::Text("License MIT.");

                    ImGui::EndGroup();
                }

                ImGui::Separator();

                ImGui::Text("Used third party software:");
                ImGui::NewLine();
                ImGui::Text("freetype");
                ImGui::Text("glfw");
                ImGui::Text("imgui");
                ImGui::Text("lua");
                ImGui::Text("lz4");
                ImGui::Text("stb libs");
                ImGui::Text("spriv reflect");
                ImGui::Text("xxhash");
                ImGui::Text("rapidjson");
                ImGui::Text("tiny obj");
                ImGui::Text("tiny gltf");
            }
            ImGui::End();
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
            AddAction<&EditorAppImpl::SaveEditingSecne>("SaveEditingSecne", ICON_FA_SAVE, "Save editing scene");
            AddAction<&EditorAppImpl::SaveAllSecnes>("SaveAllSecnes", ICON_FA_SAVE, "Save all scenes");
            AddAction<&EditorAppImpl::ToggleGameMode>("ToggleGameMode", ICON_FA_PLAY, "Toggle game mode");
            AddAction<&EditorAppImpl::SetTranslateGizmoMode>("SetTranslateGizmoMode", ICON_FA_ARROWS_ALT, "Set translate mode").
                isSelected.Bind<&Gizmo::Config::IsTranslateMode>(&gizmoConfig);
            AddAction<&EditorAppImpl::SetRotateGizmoMode>("SetRotateGizmoMode", ICON_FA_UNDO, "Set rotate mode").
                isSelected.Bind<&Gizmo::Config::IsRotateMode>(&gizmoConfig);
            AddAction<&EditorAppImpl::SetScaleGizmoMode>("SetScaleGizmoMode", ICON_FA_EXPAND_ALT, "Set scale mode").
                isSelected.Bind<&Gizmo::Config::IsScaleMode>(&gizmoConfig);
        }

        void LoadPlugins()
        {
            // TODO: Load editor plugins

            // Init plugins
            for (EditorPlugin* plugin : plugins)
                plugin->Initialize();
        }

        static void ClearAddComponentTreeNode(AddComponentTreeNode* node)
        {
            if (node != nullptr)
            {
                ClearAddComponentTreeNode(node->child);
                ClearAddComponentTreeNode(node->next);
                CJING_DELETE(node);
            }
        }

        static void InsertAddComponentTreeNodeImpl(AddComponentTreeNode& parent, AddComponentTreeNode* node)
        {
            if (parent.child == nullptr)
            {
                parent.child = node;
                return;
            }

            if (compareString(parent.child->label, node->label) > 0)
            {
                node->next = parent.child;
                parent.child = node;
                return;
            }

            auto child = parent.child;
            while (child->next && compareString(child->next->label, node->label) < 0)
                child = child->next;

            node->next = child->next;
            child->next = node;
        }

        void InsertAddComponentTreeNode(AddComponentTreeNode& parent, AddComponentTreeNode* node)
        {
            // Check children
            for (auto child = parent.child; child != nullptr; child = child->next)
            {
                if (!child->plugin && StartsWith(node->label, child->label))
                {
                    InsertAddComponentTreeNode(*child, node);
                    return;
                }
            }

            const char* rest = node->label + StringLength(parent.label);
            if (parent.label[0] != '\0') 
                ++rest; // include '/'
            int slashPos = FindSubstring(rest, "/", 0);
            if (slashPos < 0)
            {
                InsertAddComponentTreeNodeImpl(parent, node);
                return;
            }
        }

        void RegisterComponent(const char* icon, ComponentType compType, IAddComponentPlugin* plugin) override
        {
            componentLabels.insert(compType.index, plugin->GetLabel());
            if (icon && icon[0])
                componentIcons.insert(compType.index, icon);

            U32 index = 0;
            while (index < addCompPlugins.size() && compareString(plugin->GetLabel(), addCompPlugins[index]->GetLabel()) > 0)
                index++;
            addCompPlugins.Insert(index, plugin);

            auto node = CJING_NEW(AddComponentTreeNode);
            CopyString(node->label, plugin->GetLabel());
            node->plugin = plugin;
            InsertAddComponentTreeNode(addCompTreeNodeRoot, node);
        }

        void RegisterComponent(const char* icon, const char* label, ComponentType compType)
        {
            componentLabels.insert(compType.index, label);
            if (icon && icon[0])
                componentIcons.insert(compType.index, icon);

            // Create and add AddcomponentPlugin
            struct Plugin final : IAddComponentPlugin
            {
                virtual void OnGUI(bool createEntity, bool fromFilter, EditorApp& editor) override
                {
                    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

                    const char* name = label;
                    int slashPos = ReverseFindChar(label, '/');
                    if (slashPos >= 0)
                        name = label + slashPos + 1;
                    name = fromFilter ? label : name;

                    auto& sceneEditing = editor.GetSceneEditingModule();
                    if (ImGui::MenuItem(name))
                    {
                        if (createEntity)
                        {
                            ECS::Entity entity = sceneEditing.AddEmptyEntity();
                            sceneEditing.SelectEntities(Span(&entity, 1), false);
                        }
                        const auto& selectedEntities = sceneEditing.GetSelectedEntities();
                        if (selectedEntities.empty())
                            return;

                        sceneEditing.AddComponent(selectedEntities[0], compType);
                    }
                }

                const char* GetLabel() const override {
                    return label;
                }

                ComponentType compType;
                char label[64];
            };
        
            Plugin* plugin = CJING_NEW(Plugin);
            plugin->compType = compType;
            CopyString(plugin->label, label);

            U32 index = 0;
            while (index < addCompPlugins.size() && compareString(plugin->GetLabel(), addCompPlugins[index]->GetLabel()) > 0)
                index++;
            addCompPlugins.Insert(index, plugin);
            
            // Create new addComponentTreeNode
            auto node = CJING_NEW(AddComponentTreeNode);
            CopyString(node->label, plugin->GetLabel());
            node->plugin = plugin;
            InsertAddComponentTreeNode(addCompTreeNodeRoot, node);
        }

        void OnSceneEditing(Scene* scene) override
        {
            for (auto widget : widgets)
                widget->OnSceneEditing(scene);
        }

        void ClearWorld()
        {
            // Clear addComponentTree nodes
            ClearAddComponentTreeNode(addCompTreeNodeRoot.child);

            // Remove add component plugins
            for (IAddComponentPlugin* plugin : addCompPlugins)
                CJING_SAFE_DELETE(plugin);
            addCompPlugins.clear();
        }

        void InitReflection()
        {
            addCompTreeNodeRoot.label[0] = '\0';

            for (const auto& cmp : Reflection::GetComponents())
            {
                ASSERT(cmp.meta->compType != INVALID_COMPONENT_TYPE);
                const auto meta = cmp.meta;
                if (componentLabels.find(meta->compType.index).isValid())
                    continue;

                RegisterComponent(meta->icon, meta->name, meta->compType);
            }
        }

        ImFont* AddFontFromFile(const char* path, F32 fontSize, bool mergeIcons = false)
        {
            Engine& engine = GetEngine();
            OutputMemoryStream mem;
            if (!FileSystem::LoadContext(path, mem))
            {
                Logger::Error("Failed to load font %s", path);
                return nullptr;
            }

            ImGuiIO& io = ImGui::GetIO();
            ImFontConfig cfg;
            CopyString(cfg.Name, path);
            cfg.FontDataOwnedByAtlas = false;
            ImFont* font = io.Fonts->AddFontFromMemoryTTF((void*)mem.Data(), (I32)mem.Size(), fontSize, &cfg);
            if (font == nullptr)
            {
                Logger::Error("Failed to load font %s", path);
                return nullptr;
            }

            if (mergeIcons)
            {
                ImFontConfig config;
                CopyString(config.Name, "editor/fonts/fa-regular-400.ttf");
                config.MergeMode = true;
                config.FontDataOwnedByAtlas = false;
                config.GlyphMinAdvanceX = fontSize;

                static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
                OutputMemoryStream iconsData;
                if (FileSystem::LoadContext(config.Name, iconsData))
                {
                    ImFont* iconsFont = io.Fonts->AddFontFromMemoryTTF((void*)iconsData.Data(), (I32)iconsData.Size(), fontSize * 0.75f, &config, iconRanges);
                    ASSERT(iconsFont);
                }

                CopyString(config.Name, "editor/fonts/fa-solid-900.ttf");
                iconsData.Clear();
                if (FileSystem::LoadContext(config.Name, iconsData))
                {
                    ImFont* iconsFont = io.Fonts->AddFontFromMemoryTTF((void*)iconsData.Data(), (I32)iconsData.Size(), fontSize * 0.75f, &config, iconRanges);
                    ASSERT(iconsFont);
                }
            }
            return font;
        }

        void ShowComponentGizmo()
        {
            // Debug draw for selected entities
            const auto& selected = sceneEditingModule->GetSelectedEntities();
            if (!selected.empty())
            {
                auto entity = selected[0];
                entity.Each([&](ECS::EntityID compID) {
                    for (auto plugin : plugins);
                        // plugin->ShowComponentGizmo(worldEditor->GetView(), entity, compID);
                });
            }
        }

        template<void (EditorAppImpl::*Func)()>
        Utils::Action& AddAction(const char* label)
        {
            Utils::Action* action = CJING_NEW(Utils::Action)();
            action->Init(label, label);
            action->func.Bind<Func>(this);
            actions.push_back(action);
            return *action;
        }

        template<void (EditorAppImpl::* Func)()>
        Utils::Action& AddAction(const char* label, const char* icon, const char* tooltip)
        {
            Utils::Action* action = CJING_NEW(Utils::Action)();
            action->Init(label, label, tooltip, icon);
            action->func.Bind<Func>(this);
            actions.push_back(action);
            return *action;
        }

        EditorPlugin* GetPlugin(const char* name)override
        {
            for (auto plugin : plugins)
            {
                if (EqualString(plugin->GetName(), name))
                    return plugin;
            }
            return nullptr;
        }

        Utils::Action* GetAction(const char* name)override
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
            Engine::RequestExit();
        }

        void SaveEditingSecne()
        {
            sceneEditingModule->SaveEditingScene();
        }

        void SaveAllSecnes()
        {
            levelModule->SaveAllScenes();
        }

        void ToggleGameMode()
        {
            Logger::Info("Toggle game mode");
        }

        void SetTranslateGizmoMode()
        {
            gizmoConfig.mode = Gizmo::Config::Mode::TRANSLATE;
        }

        void SetRotateGizmoMode()
        {
            gizmoConfig.mode = Gizmo::Config::Mode::ROTATE;
        }

        void SetScaleGizmoMode()
        {
            gizmoConfig.mode = Gizmo::Config::Mode::SCALE;
        }

    private:
        Array<EditorPlugin*> plugins;
        Array<EditorWidget*> widgets;
        Array<Utils::Action*> actions;
        Settings settings;
        Gizmo::Config gizmoConfig;
        EditorStateMachine stateMachine;
        SplashScreen* splashScreen;

        // Windows
        Platform::WindowType mainWindow;
        Array<Platform::WindowType> windows;
        struct WindowToDestroy 
        {
            Platform::WindowType window;
            U32 counter = 0;
        };
        Array<WindowToDestroy> toDestroyWindows;
        Array<Platform::WindowEvent> windowEvents;
        bool mouseCaptured = false;

        // Render interace
        RenderInterface* renderInterface = nullptr;

        // Modules
        Array<EditorModule*> editorModules;
        ContentDatabaseModule* contentDatabase;
        ThumbnailsModule* thumbnails;
        RendererModule* rendererModule;
        LevelModule* levelModule;
        SceneEditingModule* sceneEditingModule;

        // Builtin widgets
        UniquePtr<AssetImporter> assetImporter;
        UniquePtr<AssetBrowser> assetBrowser;
        UniquePtr<LogWidget> logWidget;
        UniquePtr<PropertyWidget> propertyWidget;
        UniquePtr<EntityListWidget> entityListWidget;
        UniquePtr<RenderGraphWidget> renderGraphWidget;
        UniquePtr<ProfilerWidget> profilerWidget;

        // Reflection
        HashMap<I32, String> componentLabels;
        HashMap<I32, StaticString<5>> componentIcons;
        AddComponentTreeNode addCompTreeNodeRoot;
        Array<IAddComponentPlugin*> addCompPlugins;
        
        // Profiler
        ProfilerTools profilerTools;

        // Fonts
        ImFont* font;
        ImFont* boldFont;
        ImFont* bigIconFont;

        // Imgui
        ImGuiKey imguiKeyMap[255];
    };

    EditorApp* EditorApp::Create()
    {
		return CJING_NEW(EditorAppImpl)();
    }

    ProjectInfo* EditorApp::GetProject()
    {
        return ProjectInfo::EditorProject;
    }
    
    void EditorApp::Destroy(EditorApp* app)
    {
    }
}
}