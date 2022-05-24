#include "app.h"
#include "core\utils\log.h"
#include "core\utils\profiler.h"
#include "core\events\event.h"
#include "core\platform\platform.h"
#include "core\jobsystem\jobsystem.h"
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

    // Get renderer
    renderer = static_cast<RendererPlugin*>(engine->GetPluginManager().GetPlugin("Renderer"));
    ASSERT(renderer != nullptr);

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
    PROFILE_BLOCK("Renderer");
    renderer->Render();
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
        RequestShutdown();
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

void App::RunFrame()
{
    Profiler::BeginFrame();

    // Calculate delta time
    deltaTime = timer.Tick();
    float dt = framerateLock ? (1.0f / targetFrameRate) : deltaTime;
    ++lastTimeFrames;
    lastTimeDeltas[lastTimeFrames % LengthOf(lastTimeDeltas)] = dt;

    ComputeSmoothTimeDelta();

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