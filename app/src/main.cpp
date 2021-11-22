
//#include "app.h"
//#include "wsi.h"

#include "client\app\app.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{
class TestApp : public App
{
public:
};

App* CreateApplication(int, char**)
{
    try
    {
        App* app = new TestApp();
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