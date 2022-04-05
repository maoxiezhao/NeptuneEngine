#include "app.h"
#include "core\utils\log.h"
#include "core\utils\profiler.h"
#include "core\events\event.h"
#include "core\platform\platform.h"
#include "core\jobsystem\jobsystem.h"

namespace VulkanTest
{
static StdoutLoggerSink mStdoutLoggerSink;

App::App()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    Logger::RegisterSink(mStdoutLoggerSink);
    Logger::Info("App initialized.");
     
    Jobsystem::Initialize(Platform::GetCPUsCount());
}

App::~App()
{
    Jobsystem::Uninitialize();
}

bool App::InitializeWSI(std::unique_ptr<WSIPlatform> platform_)
{
    platform = std::move(platform_);
    wsi.SetPlatform(platform.get());

    if (!wsi.Initialize(Platform::GetCPUsCount() + 1))
        return false;

    return true;
}

void App::Initialize()
{
    // Create game engine
    Engine::InitConfig config = {};
    config.windowTitle = GetWindowTitle();
    engine = CreateEngine(config, *this);

    // Create game world
    world = &engine->CreateWorld();

    // Start game
    engine->Start(*world);
}

void App::Uninitialize()
{
    engine->Stop(*world);
    engine->DestroyWorld(*world);
    engine.Reset();
    world = nullptr;
}

void App::Render()
{
}

void App::Update(F32 deltaTime)
{
    PROFILE_BLOCK("Update");
    engine->Update(*world, deltaTime);
}

void App::FixedUpdate()
{
    engine->FixedUpdate(*world);
}

bool App::Poll()
{
    if (!platform->IsAlived(wsi))
        return false;

    if (requestedShutdown)
        return false;
    
    EventManager::Instance().Dispatch();
    return true;
}

void App::RunFrame()
{
    Profiler::BeginFrame();

    deltaTime = timer.Tick();
    float dt = framerateLock ? (1.0f / targetFrameRate) : deltaTime;

    // FixedUpdate engine
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

    // Update engine
    Update(dt);

    // Render frame
    wsi.BeginFrame();
    Render();
    wsi.EndFrame();

    Profiler::EndFrame();
}
}