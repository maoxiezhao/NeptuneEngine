#include "app.h"
#include "vulkan\device.h"

Platform::Platform()
{
}

Platform::~Platform()
{
    if (window != nullptr)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool Platform::Init(int width_, int height_)
{
    width = width_;
    height = height_;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // create window
    window = glfwCreateWindow(width, height, "Vulkan Test", nullptr, nullptr);
    if (window == nullptr)
        return false;

    return true;
}

bool Platform::Poll()
{
    glfwPollEvents();
    return !glfwWindowShouldClose(window);
}

bool App::Initialize(std::unique_ptr<Platform> platform_)
{
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

