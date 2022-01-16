
//#include "app.h"
//#include "wsi.h"

#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "renderer\renderGraph.h"

#include "core\events\event.h"
#include "core\utils\intrusiveHashMap.hpp"

namespace VulkanTest
{
    class TestApp : public App
    {
    private:
        RenderGraph renderGraph;

    public:
        TestApp(const InitConfig& initConfig_) : App(initConfig_) {}

        void Initialize() override
        {
        }

        void Uninitialize() override
        {
        }

        void Render() override
        {
            GPU::DeviceVulkan* device = wsi.GetDevice();
            assert(device != nullptr);
        }
    };

    App* CreateApplication(int, char**)
    {
        try
        {
            App::InitConfig initConfig = {};
            initConfig.windowTitle = "RenderGraphTest";

            App* app = new TestApp(initConfig);
            return app;
        }
        catch (const std::exception& e)
        {
            Logger::Error("CreateApplication() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}

class TestEvent : public VulkanTest::Event
{
public:
    DEFINE_EVENT_TYPE(TestEvent)
};

class TestEventHandler
{
public:
    int a = 0;
    bool TestEventCallback(const TestEvent& ent)
    {

        return true;
    }
};

int main(int argc, char* argv[])
{
    TestEventHandler handler;
    handler.a = 4;

    VulkanTest::EventManager::Instance().Register<TestEventHandler, TestEvent, &TestEventHandler::TestEventCallback>(&handler);
    VulkanTest::EventManager::Instance().Enqueue<TestEvent>();
    VulkanTest::EventManager::Instance().Dispatch();

    return 0;
    //return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}