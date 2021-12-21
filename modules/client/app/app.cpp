#include "app.h"
#include "core\utils\log.h"
#include "core\utils\profiler.h"

namespace VulkanTest
{
static StdoutLoggerSink mStdoutLoggerSink;

App::App(const InitConfig& initConfig_) :
    initConfig(initConfig_)
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

void App::SetPlatform(std::unique_ptr<WSIPlatform> platform_)
{
    platform = std::move(platform_);
    wsi.SetPlatform(platform.get());
}

bool App::InitializeWSI()
{
    if (!wsi.Initialize(1))
        return false;

    return true;
}

void App::Initialize()
{

}

void App::Uninitialize()
{
}

void App::Render()
{
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