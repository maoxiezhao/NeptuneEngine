#include "test.h"
#include "shaderCompiler.h"
#include "vulkan\device.h"

Shader screenVS;
Shader screenPS;
ShaderProgram* shaderProgram = nullptr;

void TestApp::InitializeImpl()
{
    auto device = mWSI.GetDevice();
    // init test shader
    ShaderCompiler::LoadShader(*device, ShaderStage::VS, screenVS, "screenVS.hlsl");
    ShaderCompiler::LoadShader(*device, ShaderStage::PS, screenPS, "screenPS.hlsl");

    ShaderProgramInfo info = {};
    info.VS = &screenVS;
    info.PS = &screenPS;
    shaderProgram = new ShaderProgram(device, info);
}

void TestApp::UninitializeImpl()
{
    delete shaderProgram;

    auto device = mWSI.GetDevice();
    if (screenVS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(device->mDevice, screenVS.mShaderModule, nullptr);
    if (screenPS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(device->mDevice, screenPS.mShaderModule, nullptr);
}

void TestApp::Render()
{
    DeviceVulkan* device = mWSI.GetDevice();
    assert(device != nullptr);

    RenderPassInfo rp = device->GetSwapchianRenderPassInfo(*mWSI.GetSwapChain(), SwapchainRenderPassType::ColorOnly);
    CommandListPtr cmd = device->RequestCommandList(QUEUE_TYPE_GRAPHICS);
    cmd->BeginRenderPass(rp);
    cmd->SetDefaultOpaqueState();
    cmd->SetProgram(shaderProgram);

    cmd->DrawIndexed(3);

    cmd->EndRenderPass();
    device->Submit(cmd);
}
