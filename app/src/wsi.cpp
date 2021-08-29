#include "wsi.h"
#include "app.h"
#include "vulkan\gpu.h"
#include "shaderCompiler.h"

#ifdef DEBUG
static bool debugLayer = true;
#else
static bool debugLayer = false;
#endif

DeviceVulkan* deviceVulkan = nullptr;
Shader screenVS;
Shader screenPS;

bool WSI::Initialize()
{
    // init gpu
    deviceVulkan = new DeviceVulkan(mPlatform->GetWindow(), debugLayer);

    // init swapchian
    if (!deviceVulkan->InitSwapchain(mPlatform->GetWidth(), mPlatform->GetHeight()))
    {
        std::cout << "Failed to init swapchian." << std::endl;
        return false;
    }

    // init test shader
    ShaderCompiler::LoadShader(*deviceVulkan, ShaderStage::VS, screenVS, "screenVS.hlsl");
    ShaderCompiler::LoadShader(*deviceVulkan, ShaderStage::PS, screenPS, "screenPS.hlsl");

	return true;
}

void WSI::Uninitialize()
{
    if (screenVS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(deviceVulkan->mDevice, screenVS.mShaderModule, nullptr);
    if (screenPS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(deviceVulkan->mDevice, screenPS.mShaderModule, nullptr);

    delete deviceVulkan;
}

void WSI::RunFrame()
{
}

void WSI::SetPlatform(Platform* platform)
{
	mPlatform = platform;
}
