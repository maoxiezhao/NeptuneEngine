
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
        RenderGraph graph;

    public:
        TestApp(const InitConfig& initConfig_) : App(initConfig_) {}

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
            if (!wsi.Initialize(1))
                return;

            GPU::DeviceVulkan* device = wsi.GetDevice();
            graph.SetDevice(device);

            ResourceDimensions dim;
            dim.width = 1280;
            dim.height = 720;
            dim.format = wsi.GetSwapchainFormat();
            graph.SetBackbufferDimension(dim);

            AttachmentInfo back;
            back.format = dim.format;
            back.sizeX = dim.width;
            back.sizeY = dim.height;

            auto& finalPass = graph.AddRenderPass("Final", RenderGraphQueueFlag::Graphics);
            finalPass.WriteColor("back", back);
            finalPass.SetBuildCallback([&](GPU::CommandList& cmd) {
                cmd.SetDefaultOpaqueState();
                cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
                cmd.Draw(3);
            });
            graph.SetBackBufferSource("back");
            graph.Bake();
            graph.Log();
        }

        void Uninitialize() override
        {
            graph.Reset();
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

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}