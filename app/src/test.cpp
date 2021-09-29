#include "test.h"
#include "vulkan\device.h"
#include "utils\log.h"

static StdoutLoggerSink mStdoutLoggerSink;

GPU::Shader* screenVS;
//Shader screenPS;
GPU::ShaderProgram* shaderProgram = nullptr;

void TestApp::InitializeImpl()
{
    Logger::RegisterSink(mStdoutLoggerSink);

    auto device = wsi.GetDevice();
    //// init test shader
    //screenVS = device->GetShaderManager();
    //ShaderCompiler::LoadShader(*device, ShaderStage::VS, screenVS, "screenVS.hlsl");
    //ShaderCompiler::LoadShader(*device, ShaderStage::PS, screenPS, "screenPS.hlsl");

    GPU::ShaderProgramInfo info = {};
    //info.VS = &screenVS;
    //info.PS = &screenPS;
    shaderProgram = new GPU::ShaderProgram(device, info);
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
    GPU::DeviceVulkan* device = wsi.GetDevice();
    assert(device != nullptr);

    GPU::RenderPassInfo rp = device->GetSwapchianRenderPassInfo(*wsi.GetSwapChain(), GPU::SwapchainRenderPassType::ColorOnly);
    GPU::CommandListPtr cmd = device->RequestCommandList(GPU::QUEUE_TYPE_GRAPHICS);
    cmd->BeginRenderPass(rp);
    cmd->SetDefaultOpaqueState();
    cmd->SetProgram(shaderProgram);

    cmd->DrawIndexed(3);

    cmd->EndRenderPass();
    device->Submit(cmd);
}
