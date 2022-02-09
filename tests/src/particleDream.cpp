
#include "client\app\app.h"
#include "client\app\engine.h"
#include "gpu\vulkan\device.h"
#include "math\math.hpp"
#include "core\memory\memory.h"

namespace VulkanTest
{
    class TestApp : public App
    {
    private:
        UniquePtr<Engine> engine;

    private:
        Vec2 vertices[6] = {
            Vec2(-0.5f, -0.5f),
            Vec2(-0.5f, +0.5f),
            Vec2(+0.5f, +0.5f),
            Vec2(+0.5f, +0.5f),
            Vec2(+0.5f, -0.5f),
            Vec2(-0.5f, -0.5f)
        };

        Vec2 uvs[6] = {
            Vec2(0.0f,  0.0f),
            Vec2(0.0f, 1.0f),
            Vec2(1.0f, 1.0f),
            Vec2(1.0f, 1.0f),
            Vec2(1.0f, 0.0f),
            Vec2(0.0f, 0.0f)
        };

        GPU::ImagePtr images[4];

        struct PushConstantImage
        {
            int textureIndex = 2;
        };
        PushConstantImage push;

    public:
        TestApp() : App() {}

        void Initialize() override
        {
            // Initialize WSI
            InitializeWSI();

            // App start
            OnStart();
        }

        void Uninitialize()
        {
            OnStop();
        }

    private:
        void OnStart()
        {
            GPU::TextureFormatLayout formatLayout;
            formatLayout.SetTexture2D(VK_FORMAT_R8G8B8A8_SRGB, 1, 1);

            GPU::DeviceVulkan* device = wsi.GetDevice();
            GPU::ImageCreateInfo imageInfo = GPU::ImageCreateInfo::ImmutableImage2D(1, 1, VK_FORMAT_R8G8B8A8_SRGB);
            GPU::SubresourceData data = {};
            data.rowPitch = formatLayout.RowByteStride(1);

            const uint8_t red[] = { 0xff, 0, 0, 0xff };
            const uint8_t green[] = { 0, 0xff, 0, 0xff };
            const uint8_t blue[] = { 0, 0, 0xff, 0xff };
            const uint8_t black[] = { 0, 0, 0, 0xff };

            data.data = red;
            images[0] = device->CreateImage(imageInfo, &data);
            data.data = green;
            images[1] = device->CreateImage(imageInfo, &data);
            data.data = blue;
            images[2] = device->CreateImage(imageInfo, &data);
            data.data = black;
            images[3] = device->CreateImage(imageInfo, &data);

            device->SetName(*images[0], "ColorImg0");
            device->SetName(*images[1], "ColorImg1");
            device->SetName(*images[2], "ColorImg2");
            device->SetName(*images[3], "ColorImg3");
        }

        void OnStop()
        {
            for (int i = 0; i < 4; i++)
            {
                if (images[i])
                    images[i].reset();
            }
        }

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
                cmd->SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

                GPU::BindlessDescriptorPoolPtr bindlessPoolPtr = device->GetBindlessDescriptorPool(GPU::BindlessReosurceType::SampledImage, 1, 1024);
                if (bindlessPoolPtr)
                {
                    bindlessPoolPtr->AllocateDescriptors(1024);
                    for (int i = 0; i < 1024; i++)
                        bindlessPoolPtr->SetTexture(i, images[i % 4]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                    cmd->SetBindless(1, bindlessPoolPtr->GetDescriptorSet());
                    cmd->SetSampler(0, 0, GPU::StockSampler::NearestClamp);
                    cmd->PushConstants(&push, 0, sizeof(push));

                    Vec2* vertBuf = static_cast<Vec2*>(cmd->AllocateVertexBuffer(0, sizeof(vertices), sizeof(Vec2), VK_VERTEX_INPUT_RATE_VERTEX));
                    Vec2* uvBuf = static_cast<Vec2*>(cmd->AllocateVertexBuffer(1, sizeof(uvs), sizeof(Vec2), VK_VERTEX_INPUT_RATE_VERTEX));
                    memcpy(vertBuf, vertices, sizeof(vertices));
                    memcpy(uvBuf, uvs, sizeof(uvs));
                    cmd->SetVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, 0);
                    cmd->SetVertexAttribute(1, 1, VK_FORMAT_R32G32_SFLOAT, 0);

                    cmd->Draw(6);
                }
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