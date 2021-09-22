#include "test.h"
#include "vulkan\device.h"
#include "utils\log.h"

static StdoutLoggerSink mStdoutLoggerSink;

Shader* screenVS;
//Shader screenPS;
ShaderProgram* shaderProgram = nullptr;

void TestApp::InitializeImpl()
{
    Logger::RegisterSink(mStdoutLoggerSink);

    auto device = wsi.GetDevice();
    //// init test shader
    //screenVS = device->GetShaderManager();
    //ShaderCompiler::LoadShader(*device, ShaderStage::VS, screenVS, "screenVS.hlsl");
    //ShaderCompiler::LoadShader(*device, ShaderStage::PS, screenPS, "screenPS.hlsl");

    ShaderProgramInfo info = {};
    //info.VS = &screenVS;
    //info.PS = &screenPS;
    shaderProgram = new ShaderProgram(device, info);
}

void TestApp::UninitializeImpl()
{
    delete shaderProgram;

    auto device = wsi.GetDevice();
    //if (screenVS.mShaderModule != VK_NULL_HANDLE)
    //    vkDestroyShaderModule(device->device, screenVS.mShaderModule, nullptr);
    //if (screenPS.mShaderModule != VK_NULL_HANDLE)
    //    vkDestroyShaderModule(device->device, screenPS.mShaderModule, nullptr);
}

void TestApp::Render()
{
    DeviceVulkan* device = wsi.GetDevice();
    assert(device != nullptr);

    RenderPassInfo rp = device->GetSwapchianRenderPassInfo(*wsi.GetSwapChain(), SwapchainRenderPassType::ColorOnly);
    CommandListPtr cmd = device->RequestCommandList(QUEUE_TYPE_GRAPHICS);
    cmd->BeginRenderPass(rp);
    cmd->SetDefaultOpaqueState();
    cmd->SetProgram(shaderProgram);

    cmd->DrawIndexed(3);

    cmd->EndRenderPass();
    device->Submit(cmd);
}
