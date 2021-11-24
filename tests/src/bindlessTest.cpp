
//#include "app.h"
//#include "wsi.h"

#include "client\app\app.h"
#include "gpu\vulkan\device.h"

namespace VulkanTest
{
    class TestApp : public App
    {
    private:
        GPU::ImagePtr images[4];

    public:
        void InitializeImpl() override
        {
            GPU::TextureFormatLayout formatLayout;
            formatLayout.SetTexture2D(VK_FORMAT_R8G8B8A8_SRGB, 1, 1);

            GPU::DeviceVulkan *device = wsi.GetDevice();
            GPU::ImageCreateInfo imageInfo = GPU::ImageCreateInfo::ImmutableImage2D(1, 1, VK_FORMAT_R8G8B8A8_SRGB);
            GPU::SubresourceData data = {};
            data.rowPitch = formatLayout.RowByteStride(1);

            const uint8_t red[] = {0xff, 0, 0, 0xff};
            const uint8_t green[] = {0, 0xff, 0, 0xff};
            const uint8_t blue[] = {0, 0, 0xff, 0xff};
            const uint8_t black[] = {0, 0, 0, 0xff};

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

        void UninitializeImpl() override
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

            GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(GPU::SwapchainRenderPassType::ColorOnly);
            GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
            cmd->BeginEvent("Fullscreen");
            cmd->BeginRenderPass(rp);

            GPU::BindlessDescriptorPoolPtr bindlessPoolPtr = device->GetBindlessDescriptorPool(GPU::BindlessReosurceType::SampledImage, 1, 1024);
            if (bindlessPoolPtr)
            {
                bindlessPoolPtr->AllocateDescriptors(1024);
                for (int i = 0; i < 1024; i++)
                    bindlessPoolPtr->SetTexture(i, images[i % 4]->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                cmd->SetBindless(1, bindlessPoolPtr->GetDescriptorSet());
                cmd->SetBindless(2, bindlessPoolPtr->GetDescriptorSet());
                cmd->SetSampler(0, 0, GPU::StockSampler::NearestClamp);

                cmd->SetDefaultOpaqueState();
                cmd->SetProgram("screenVS.hlsl", "test/bindlessPS.hlsl");
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