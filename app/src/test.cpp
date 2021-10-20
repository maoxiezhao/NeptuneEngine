#include "test.h"
#include "vulkan\device.h"
#include "utils\log.h"

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

void TestApp::UninitializeImpl()
{
}

void TestApp::Render()
{
    GPU::DeviceVulkan* device = wsi.GetDevice();
    assert(device != nullptr);

    GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(*wsi.GetSwapChain(), GPU::SwapchainRenderPassType::ColorOnly);
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
