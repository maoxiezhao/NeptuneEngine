#include "test.h"
#include "wsi.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\definition.h"
#include "core\utils\log.h"

static StdoutLoggerSink mStdoutLoggerSink;

void TestApp::Setup()
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

static GPU::BufferPtr buffer;
void TestApp::InitializeImpl()
{
    GPU::BufferCreateInfo info = {};
    info.domain = GPU::BufferDomain::Device;
    info.size = 128;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    info.misc = 0;
    buffer = wsi.GetDevice()->CreateBuffer(info, nullptr);
}

void TestApp::UninitializeImpl()
{
    buffer.reset();
}

void TestApp::Render()
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
