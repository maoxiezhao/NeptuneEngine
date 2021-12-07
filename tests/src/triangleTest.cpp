
//#include "app.h"
//#include "wsi.h"

#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"

namespace VulkanTest
{
    class TestApp : public App
    {
    private:
        Vec2 vertices[3] = {
            Vec2(-0.5f, -0.5f),
            Vec2(-0.5f, +0.5f),
            Vec2(+0.5f, +0.5f)
        };

    public:
        void Render() override
        {
            GPU::DeviceVulkan *device = wsi.GetDevice();
            assert(device != nullptr);

            GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
            cmd->BeginEvent("TriangleTest");
            GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(GPU::SwapchainRenderPassType::ColorOnly);
            cmd->BeginRenderPass(rp);
            {
                cmd->SetProgram("test/triangleVS.hlsl", "test/trianglePS.hlsl");
                cmd->SetDefaultOpaqueState();
                cmd->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

                Vec2* vertBuf = static_cast<Vec2*>(cmd->AllocateVertexBuffer(0, sizeof(vertices), sizeof(Vec2), VK_VERTEX_INPUT_RATE_VERTEX));
                memcpy(vertBuf, vertices, sizeof(vertices));
                cmd->SetVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
                cmd->Draw(3);
            }
            cmd->EndRenderPass();
            cmd->EndEvent();

            device->Submit(cmd);
        }
    };

    App *CreateApplication(int, char **)
    {
        try
        {
            App *app = new TestApp();
            return app;
        }
        catch (const std::exception &e)
        {
            Logger::Error("CreateApplication() threw exception: %s\n", e.what());
            return nullptr;
        }
    }
}

int main(int argc, char *argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}