
#include "client\app\app.h"
#include "gpu\vulkan\device.h"
#include "renderer\renderGraph.h"
#include "core\platform\platform.h"
#include "core\events\event.h"
#include "core\utils\intrusiveHashMap.hpp"
#include "math\random.h"

namespace VulkanTest
{
    class TestApp : public App
    {
    private:
        RenderGraph graph;

        Vec2 vertices[4] = {
            Vec2(-0.5f, -0.5f),
            Vec2(-0.5f, +0.5f),
            Vec2(+0.5f, +0.5f),
            Vec2(+0.5f, -0.5f),
        };

        Vec2 uvs[4] = {
            Vec2(0.0f, 0.0f),
            Vec2(0.0f, 1.0f),
            Vec2(1.0f, 1.0f),
            Vec2(1.0f, 0.0f),
        };

        U32 indices[6] = {
            0, 1, 2,
            0, 2, 3
        };

    public:
        TestApp(const InitConfig& initConfig_) : App(initConfig_) 
        {
            Jobsystem::Initialize(Platform::GetCPUsCount() - 1);
        }

        ~TestApp()
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
        
        RenderTextureResource* colors[4];
        void Initialize() override
        {
            if (!wsi.Initialize(Platform::GetCPUsCount()))
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
            back.sizeX = (F32)dim.width;
            back.sizeY = (F32)dim.height;

            AttachmentInfo color;
            color.format = VK_FORMAT_R8G8B8A8_SRGB;
            color.sizeX = 1.0f;
            color.sizeY = 1.0f;
            color.samples = VK_SAMPLE_COUNT_1_BIT;

            // Color pass
            for (int i = 0; i < 4; i++)
            {
                String colorName = "color" + std::to_string(i);
                String passName = "ColorPass" + std::to_string(i);
                auto& colorPass = graph.AddRenderPass(passName.c_str(), RenderGraphQueueFlag::Graphics);
                colors[i] = &colorPass.WriteColor(colorName, color);
                colorPass.SetClearColorCallback([](U32 index, VkClearColorValue* value) {
                    if (value != nullptr)
                    {
                        value->float32[0] = 0.0f;
                        value->float32[1] = 0.0f;
                        value->float32[2] = 0.0f;
                        value->float32[3] = 1.0f;
                    }
                    return true;
                });
                colorPass.SetBuildCallback([&](GPU::CommandList& cmd) {
                    cmd.SetDefaultOpaqueState();
                    cmd.SetProgram("screenVS.hlsl", "screenPS.hlsl");
                    cmd.Draw(3);
                });
            }

            // Final pass
            auto& finalPass = graph.AddRenderPass("Final", RenderGraphQueueFlag::Graphics);
            finalPass.ReadTexture("color0");
            finalPass.ReadTexture("color1");
            finalPass.ReadTexture("color2");
            finalPass.ReadTexture("color3");
            finalPass.WriteColor("back", back);
            finalPass.SetClearColorCallback([](U32 index, VkClearColorValue* value) {
                if (value != nullptr)
                {
                    value->float32[0] = 0.0f;
                    value->float32[1] = 0.0f;
                    value->float32[2] = 0.0f;
                    value->float32[3] = 1.0f;
                }
                return true;
            });
            finalPass.SetBuildCallback([&](GPU::CommandList& cmd) 
            {
                cmd.SetProgram("test/triangleVS.hlsl", "test/trianglePS.hlsl");
                cmd.SetDefaultOpaqueState();
                cmd.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

                GPU::DeviceVulkan* device = GetWSI().GetDevice();
                GPU::BindlessDescriptorPoolPtr bindlessPoolPtr = device->GetBindlessDescriptorPool(GPU::BindlessReosurceType::SampledImage, 1, 1024);
                if (bindlessPoolPtr)
                {
                    bindlessPoolPtr->AllocateDescriptors(16);
                    bindlessPoolPtr->SetTexture(0, graph.GetPhysicalTexture(*colors[0]), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    bindlessPoolPtr->SetTexture(1, graph.GetPhysicalTexture(*colors[1]), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    bindlessPoolPtr->SetTexture(2, graph.GetPhysicalTexture(*colors[2]), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    bindlessPoolPtr->SetTexture(3, graph.GetPhysicalTexture(*colors[3]), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                    cmd.SetBindless(1, bindlessPoolPtr->GetDescriptorSet());
                    cmd.SetSampler(0, 0, GPU::StockSampler::NearestClamp);
                 
                    Vec2* vertBuf = static_cast<Vec2*>(cmd.AllocateVertexBuffer(0, sizeof(vertices), sizeof(Vec2), VK_VERTEX_INPUT_RATE_VERTEX));
                    Vec2* uvBuf = static_cast<Vec2*>(cmd.AllocateVertexBuffer(1, sizeof(uvs), sizeof(Vec2), VK_VERTEX_INPUT_RATE_VERTEX));
                    memcpy(vertBuf, vertices, sizeof(vertices));
                    memcpy(uvBuf, uvs, sizeof(uvs));
                    cmd.SetVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
                    cmd.SetVertexAttribute(1, 1, VK_FORMAT_R32G32_SFLOAT, 0);
                    
                    U32* indexBuffer = static_cast<U32*>(cmd.AllocateIndexBuffer(sizeof(indices), VK_INDEX_TYPE_UINT32));
                    memcpy(indexBuffer, indices, sizeof(indices));

                    cmd.DrawIndexed(6);
                }
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
            graph.SetupAttachments(*device, &device->GetSwapchainView());
            Jobsystem::JobHandle handle = Jobsystem::INVALID_HANDLE;
            graph.Render(*device, handle);
            Jobsystem::Wait(handle);

            device->MoveReadWriteCachesToReadOnly();
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

using namespace VulkanTest;

int main(int argc, char* argv[])
{
    return VulkanTest::ApplicationMain(VulkanTest::CreateApplication, argc, argv);
}