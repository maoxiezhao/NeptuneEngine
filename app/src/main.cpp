
#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "renderer\renderPath3D.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
    class MainRenderer : public RenderPath3D {};

    class MainApp : public App
    {
    public:
        U32 GetDefaultWidth() override
        {
            return 1280;
        }

        U32 GetDefaultHeight() override
        {
            return 720;
        }

        void Initialize() override
        {
            App::Initialize();
            renderer->ActivePath(&mainRenderer);
        }

    private:
        MainRenderer mainRenderer;
    };

    App* CreateApplication(int, char**)
    {
        try
        {
            App* app = new MainApp();
            return app;
        }
        catch (const std::exception& e)
        {
            Logger::Error("CreateApplication() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}

using namespace VulkanTest;

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}