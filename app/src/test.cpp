#include "test.h"
#include "vulkan\device.h"
#include "utils\log.h"

static StdoutLoggerSink mStdoutLoggerSink;

void TestApp::InitializeImpl()
{
    Logger::RegisterSink(mStdoutLoggerSink);
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
    cmd->BeginRenderPass(rp);
    cmd->SetDefaultOpaqueState();
    cmd->SetProgram("screenVS.hlsl", "screenPS.hlsl");
    cmd->DrawIndexed(3);
    cmd->EndRenderPass();
    device->Submit(cmd);
}
