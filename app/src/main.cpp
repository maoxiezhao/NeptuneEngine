
#include "app.h"
#include "wsi.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\definition.h"
#include "core\utils\log.h"

static StdoutLoggerSink mStdoutLoggerSink;

class TestApp : public App
{
private:
    GPU::ImagePtr images[4];

public:
    void Setup()override
    {
        // console for std output..
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }

        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        Logger::RegisterSink(mStdoutLoggerSink);

        Logger::Info("Test app initialize.");
    }
    
    void InitializeImpl()override
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
    }
    
    void UninitializeImpl()override
    {
        for (int i = 0; i < 4; i++)
        {
            if (images[i])
                images[i].reset();
        }
    }

    void Render()override
    {
        GPU::DeviceVulkan* device = wsi.GetDevice();
        assert(device != nullptr);

        GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(GPU::SwapchainRenderPassType::ColorOnly);
        GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);

        cmd->BeginEvent("Fullscreen");
        cmd->BeginRenderPass(rp);
        cmd->SetDefaultOpaqueState();
        cmd->SetProgram("screenVS.hlsl", "screenPS.hlsl");
        cmd->Draw(3);
        cmd->EndRenderPass();
        cmd->EndEvent();

        device->Submit(cmd);
    }
};

int main()
{
    std::unique_ptr<TestApp> app = std::make_unique<TestApp>();
    if (app == nullptr)
        return 0;

    std::unique_ptr<Platform> platform = std::make_unique<Platform>();
    if (!platform->Init(800, 600))
        return 0;

    if (!app->Initialize(std::move(platform)))
        return 0;

    while (app->Poll())
        app->Tick();

    app->Uninitialize();
    app.reset();

    return 0;
}