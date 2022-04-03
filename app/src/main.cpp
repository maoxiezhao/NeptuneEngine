
#include "client\app\app.h"
#include "core\platform\platform.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"

namespace VulkanTest
{
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
        }

        void Uninitialize() override
        {       
            App::Uninitialize();
        }

        void Render() override
        {
            //GPU::DeviceVulkan* device = wsi.GetDevice();
            //assert(device != nullptr);

            //GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(GPU::SwapchainRenderPassType::ColorOnly);
            //GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
            //cmd->BeginEvent("Fullscreen");
            //cmd->BeginRenderPass(rp);

            //cmd->EndRenderPass();
            //cmd->EndEvent();
            //device->Submit(cmd);
        }
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