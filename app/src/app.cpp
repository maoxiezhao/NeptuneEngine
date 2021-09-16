#include "app.h"
#include "vulkan\device.h"

Platform::Platform()
{
}

Platform::~Platform()
{
    if (mWindow != nullptr)
    {
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }
}

bool Platform::Init(int width, int height)
{
    mWidth = width;
    mHeight = height;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    // create window
    mWindow = glfwCreateWindow(width, height, "Vulkan Test", nullptr, nullptr);
    if (mWindow == nullptr)
        return false;

    return true;
}

bool Platform::Poll()
{
    glfwPollEvents();
    return !glfwWindowShouldClose(mWindow);
}

bool App::Initialize(std::unique_ptr<Platform> platform)
{
	mPlatform = std::move(platform);
    mWSI.SetPlatform(mPlatform.get());
    
    if (!mWSI.Initialize())
        return false;

    InitializeImpl();

    return true;
}

void App::Uninitialize()
{
    UninitializeImpl();
    mWSI.Uninitialize();
}

bool App::Poll()
{
    return mPlatform->Poll();
}

void App::Tick()
{
    mWSI.BeginFrame();
    Render();
    mWSI.EndFrame();
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

