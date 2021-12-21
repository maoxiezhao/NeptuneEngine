
//#include "app.h"
//#include "wsi.h"

#include "client\app\app.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{

App* CreateApplication(int, char**)
{
    try
    {
        App::InitConfig initConfig = {};
        initConfig.windowTitle = "ParticleDream";

        App* app = new App(initConfig);
        return app;
    }
    catch (const std::exception& e)
    {
        Logger::Error("CreateApplication() threw exception: %s\n", e.what());
        return nullptr;
    }
}
}

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}