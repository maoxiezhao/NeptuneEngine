#include "app.h"
#include "core\globals.h"
#include "core\commandLine.h"
#include "core\utils\log.h"
#include "core\profiler\profiler.h"
#include "core\events\event.h"
#include "core\platform\platform.h"
#include "core\threading\jobsystem.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
static StdoutLoggerSink mStdoutLoggerSink;

App::App()
{
    for (float& f : lastTimeDeltas) 
        f = 1 / 60.f;

    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    Logger::RegisterSink(mStdoutLoggerSink);
    Logger::Info("App initialized.");
     
    Jobsystem::Initialize(Platform::GetCPUsCount());

    Platform::Initialize();
    Platform::LogPlatformInfo();

    World::SetupWorld();
}

App::~App()
{
    Platform::Uninitialize();
    Jobsystem::Uninitialize();
}

void App::Run(std::unique_ptr<WSIPlatform> platform_)
{
    platform = std::move(platform_);

    Profiler::SetThreadName("MainThread");
    Semaphore semaphore(0, 1);
    struct Data
    {
        App* app;
        Semaphore* semaphore;
    }
    data = { this, &semaphore };

    Jobsystem::Run(&data, [](void* ptr) 
    {
        Profiler::SetThreadName("AsyncMainThread");
        Globals::MainThreadID = Platform::GetCurrentThreadID();

        Data* data = static_cast<Data*>(ptr);
        App* app = data->app;
        app->Initialize();

        while (app->Poll())
            app->OnIdle();

        app->Uninitialize();
        data->semaphore->Signal();
    }, nullptr, 0);

    PROFILE_BLOCK("sleeping");
    semaphore.Wait();
}

void App::Initialize()
{
    // Init engine paths
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

    // Init platform
    bool ret = platform->Init(GetDefaultWidth(), GetDefaultHeight(), GetWindowTitle());
    ASSERT(ret);

    // Create game engine
    engine = CreateEngine(*this);

    // Load plugins
    engine->LoadPlugins();

    // Create game world
    world = &engine->CreateWorld();

    // Get renderer
    renderer = static_cast<RendererPlugin*>(engine->GetPluginManager().GetPlugin("Renderer"));
    ASSERT(renderer != nullptr);

    // Start game
    engine->Start(world);
}

void App::Uninitialize()
{
    engine->Stop(world);
    engine->DestroyWorld(world);
    engine.Reset();
    world = nullptr;
    platform.reset();
}

void App::Render()
{
    PROFILE_FUNCTION();

    F32 beforeDrawTime = timer.GetTimeSinceTick();
    GetWSI().BeginFrame();
    if (GetActivePath())
        GetActivePath()->Render();

    GetWSI().EndFrame();
    drawTime = timer.GetTimeSinceTick() - beforeDrawTime;

    // Calculate FPS
    fpsFrames++;
    if (fpsTimer.GetTimeSinceTick() > 1.0f)
    {
        fps = fpsFrames / fpsTimer.Tick();
        fpsFrames = 0;
    }
}

void App::ComputeSmoothTimeDelta()
{
    F32 tmp[11];
    memcpy(tmp, lastTimeDeltas, sizeof(tmp));
    qsort(tmp, LengthOf(tmp), sizeof(tmp[0]), [](const void* a, const void* b) -> I32 {
        return *(const float*)a < *(const float*)b ? -1 : *(const float*)a > *(const float*)b ? 1 : 0;
    });

    F32 t = 0;
    for (U32 i = 2; i < LengthOf(tmp) - 2; ++i) {
        t += tmp[i];
    }
    smoothTimeDelta = t / (LengthOf(tmp) - 4);
}

void App::ActivePath(RenderPath* renderPath_)
{
    renderPath = renderPath_;
    renderPath->SetWSI(&GetWSI());
}

void App::Update(F32 deltaTime)
{
    PROFILE_BLOCK("Update");
    engine->Update(world, deltaTime);

    if (GetActivePath()) {
        GetActivePath()->Update(deltaTime);
    }
}

void App::LateUpate()
{
    PROFILE_BLOCK("LateUpate");
    engine->LateUpdate(world);
}

void App::FixedUpdate()
{
    engine->FixedUpdate(world);

    if (GetActivePath()) {
        GetActivePath()->FixedUpdate();
    }
}

void App::FrameEnd()
{
}

bool App::Poll()
{
    if (Engine::ShouldExit())
        return false;
    
    Platform::WindowEvent ent;
    while (Platform::GetWindowEvent(ent))
        OnEvent(ent);

    EventManager::Instance().Dispatch();
    return true;
}

void App::OnEvent(const Platform::WindowEvent& ent)
{
    switch (ent.type) 
    {
    case Platform::WindowEvent::Type::QUIT:
    case Platform::WindowEvent::Type::WINDOW_CLOSE:
        Engine::RequestExit();
        break;
    case Platform::WindowEvent::Type::WINDOW_MOVE:
    case Platform::WindowEvent::Type::WINDOW_SIZE:
    {
        const auto r = Platform::GetClientBounds(platform->GetWindow());
        platform->NotifyResize(r.width, r.height);
    }
        break;
    default: 
        break;
    }
}

void App::OnIdle()
{
    GetWSI().BeginFrame();

    Profiler::BeginFrame();
    ProfilerGPU::BeginFrame();

    // Calculate delta time
    deltaTime = timer.Tick();
    float dt = framerateLock ? (1.0f / targetFrameRate) : deltaTime;
    ++lastTimeFrames;
    lastTimeDeltas[lastTimeFrames % LengthOf(lastTimeDeltas)] = dt;
    ComputeSmoothTimeDelta();

    // Update
    F32 beforeUpdateTime = timer.GetTimeSinceTick();
    Update(dt);
    LateUpate();
    updateTime = timer.GetTimeSinceTick() - beforeUpdateTime;

    // Fixed update
    Profiler::BeginBlock("FixedUpdate");
    {
        if (frameskip)
        {
            deltaTimeAcc += dt;
            if (deltaTimeAcc >= 8.0f)
                deltaTimeAcc = 0.0f;

            const F32 invTargetFrameRate = 1.0f / targetFrameRate;
            while (deltaTimeAcc >= invTargetFrameRate)
            {
                FixedUpdate();
                deltaTimeAcc -= invTargetFrameRate;
            }
        }
        else {
            FixedUpdate();
        }
    }
    Profiler::EndBlock();

    // Render
    Render();

    // End frame
    ProfilerGPU::EndFrame();
    Profiler::EndFrame();
    FrameEnd();

    GetWSI().EndFrame();
}
}