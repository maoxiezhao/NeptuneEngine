#include "app.h"
#include "core\utils\log.h"
#include "core\utils\profiler.h"

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
    Logger::Info("App initialize.");
}

App::~App()
{
}

bool App::InitWSI(std::unique_ptr<WSIPlatform> platform_)
{
	platform = std::move(platform_);
    wsi.SetPlatform(platform.get());
    
    if (!wsi.Initialize())
        return false;

    return true;
}

void App::Initialize()
{
    InitializeImpl();
}

void App::Uninitialize()
{
    UninitializeImpl();
}

bool App::Poll()
{
    if (!platform->IsAlived())
        return false;

    if (requestedShutdown)
        return false;
    
    return true;
}

void App::RunFrame()
{
    wsi.BeginFrame();
    Render();
    wsi.EndFrame();
}
}