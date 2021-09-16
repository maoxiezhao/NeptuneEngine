#include "wsi.h"
#include "app.h"
#include "vulkan\device.h"
#include "shaderCompiler.h"

#ifdef DEBUG
static bool debugLayer = true;
#else
static bool debugLayer = false;
#endif

DeviceVulkan* deviceVulkan = nullptr;
Swapchain* swapchian = nullptr;

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

	return true;
}

void WSI::Uninitialize()
{
    if (swapchian != nullptr)
        delete swapchian;

    delete deviceVulkan;
}

void WSI::BeginFrame()
{
    deviceVulkan->BeginFrameContext();

    if (swapchian->mSwapChain == VK_NULL_HANDLE)
    {
        std::cout << "Lost swapchain." << std::endl;
        return;
    }

    SemaphorePtr acquire = deviceVulkan->RequestSemaphore();

    // acquire next image index
    VkResult res = vkAcquireNextImageKHR(
        deviceVulkan->mDevice,
        swapchian->mSwapChain,
        0xFFFFFFFFFFFFFFFF,
        acquire->GetSemaphore(),
        VK_NULL_HANDLE,
        &swapchian->mImageIndex
    );
    assert(res == VK_SUCCESS);

    // acquire image to render
    acquire->Signal();

    // set swapchain acquire semaphore
    deviceVulkan->SetAcquireSemaphore(swapchian->mImageIndex, acquire);
}

void WSI::EndFrame()
{
    deviceVulkan->EndFrameContext();

    // release在EndFrameContext中设置,确保image已经释放
    SemaphorePtr release = deviceVulkan->GetAndConsumeReleaseSemaphore();
    assert(release->IsSignalled());

    // present 
    VkResult result = VK_SUCCESS;
    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &release->GetSemaphore();
    info.swapchainCount = 1;
    info.pSwapchains = &swapchian->mSwapChain;
    info.pImageIndices = &swapchian->mImageIndex;
    info.pResults = &result;

    VkResult overall = vkQueuePresentKHR(deviceVulkan->GetPresentQueue(), &info);
    if (overall >= 0)
    {
        // TODO
    }
}

void WSI::SetPlatform(Platform* platform)
{
	mPlatform = platform;
}

Swapchain* WSI::GetSwapChain()
{
    return swapchian;
}

DeviceVulkan* WSI::GetDevice()
{
    return deviceVulkan;
}
