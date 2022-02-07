
#include "client\app\app.h"
#include "client\app\engine.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"

namespace VulkanTest
{
    class SceneViewerApp : public App
    {
    private:
        U32 numThreads = 1;

    public:
        SceneViewerApp(const InitConfig& initConfig_) : App(initConfig_)
        {
            numThreads = Platform::GetCPUsCount();
            Jobsystem::Initialize(numThreads - 1);
        }

        ~SceneViewerApp()
        {
            Jobsystem::Uninitialize();
        }

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
            if (!wsi.Initialize(numThreads))
                return;
        }

        void Uninitialize() override
        {
           
        }

        void Render() override
        {
        }
    };

    App* CreateApplication(int, char**)
    {
        try
        {
            App::InitConfig initConfig = {};
            initConfig.windowTitle = "SceneViewer";

            App* app = new SceneViewerApp(initConfig);
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