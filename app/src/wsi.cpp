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
Swapchain* swapchian = nullptr;
Shader screenVS;
Shader screenPS;

bool WSI::Initialize()
{
    // init gpu
    deviceVulkan = new DeviceVulkan(mPlatform->GetWindow(), debugLayer);

    // init swapchian
    if (!deviceVulkan->CreateSwapchain(swapchian, mPlatform->GetWidth(), mPlatform->GetHeight()))
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
    if (swapchian != nullptr)
        delete swapchian;

    if (screenVS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(deviceVulkan->mDevice, screenVS.mShaderModule, nullptr);
    if (screenPS.mShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(deviceVulkan->mDevice, screenPS.mShaderModule, nullptr);

    delete deviceVulkan;
}

void WSI::BeginFrame()
{
    deviceVulkan->BeginFrameContext();
}

void WSI::EndFrame()
{
    deviceVulkan->EndFrameContext();
}

void WSI::SetPlatform(Platform* platform)
{
	mPlatform = platform;
}

DeviceVulkan* WSI::GetDevice()
{
    return deviceVulkan;
}
