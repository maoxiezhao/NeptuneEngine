#include "app.h"
#include "core\utils\log.h"

namespace VulkanTest
{
static StdoutLoggerSink mStdoutLoggerSink;

void App::Setup()
{
    //console for std output..
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }

    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    Logger::RegisterSink(mStdoutLoggerSink);
    Logger::Info("App initialize.");
}

bool App::Initialize(std::unique_ptr<WSIPlatform> platform_)
{
    Setup();

	platform = std::move(platform_);
    wsi.SetPlatform(platform.get());
    
    if (!wsi.Initialize())
        return false;

    InitializeImpl();

    return true;
}

void App::Uninitialize()
{
    UninitializeImpl();
    wsi.Uninitialize();
}

bool App::Poll()
{
    return platform->Poll();
}

void App::Tick()
{
    wsi.BeginFrame();
    Render();
    wsi.EndFrame();
}

void App::InitializeImpl()
{
}

void App::Render()
{
}

void App::UninitializeImpl()
{
}
}