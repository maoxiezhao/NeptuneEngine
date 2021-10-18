#include "wsi.h"
#include "app.h"
#include "vulkan\device.h"
#include "vulkan\context.h"

namespace {
#ifdef DEBUG
    static bool debugLayer = true;
#else
    static bool debugLayer = false;
#endif

    GPU::VulkanContext* vulkanContext = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    GPU::DeviceVulkan* deviceVulkan = nullptr;
    GPU::Swapchain* swapchian = nullptr;
    bool swapchainIndexHasAcquired = false;

    std::vector<const char*> GetRequiredExtensions(bool debugUtils)
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (debugUtils) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    std::vector<const char*> GetRequiredDeviceExtensions()
    {
        return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    }

    VkSurfaceKHR CreateSurface(VkInstance instance, Platform& platform)
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(instance, platform.GetWindow(), nullptr, &surface) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        int actual_width, actual_height;
        glfwGetFramebufferSize(platform.GetWindow(), &actual_width, &actual_height);
        platform.SetSize(
            unsigned(actual_width),
            unsigned(actual_height)
        );
        return surface;
    }
}

bool WSI::Initialize()
{
    // init vulkan context
    vulkanContext = new GPU::VulkanContext();

    auto instanceExt = GetRequiredExtensions(true);
    auto deviceExt = GetRequiredDeviceExtensions();
    if (!vulkanContext->Initialize(instanceExt, deviceExt, true))
        return false;

    // init gpu
    deviceVulkan = new GPU::DeviceVulkan();
    deviceVulkan->SetContext(*vulkanContext);

    // init surface
    surface = CreateSurface(vulkanContext->GetInstance(), *platform);
    if (surface == VK_NULL_HANDLE)
        return false;
  
    // init swapchian
    if (!deviceVulkan->CreateSwapchain(swapchian, surface, platform->GetWidth(), platform->GetHeight()))
    {
        Logger::Error("Failed to init swapchian.");
        return false;
    }

	return true;
}

void WSI::Uninitialize()
{
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(vulkanContext->GetInstance(), surface, nullptr);

    if (swapchian != nullptr)
        delete swapchian;

    delete deviceVulkan;
    delete vulkanContext;
}

void WSI::BeginFrame()
{
    deviceVulkan->BeginFrameContext();

    if (swapchian->swapChain == VK_NULL_HANDLE)
    {
        std::cout << "Lost swapchain." << std::endl;
        return;
    }

    // 当前swapchian已经Acquired
    if (swapchainIndexHasAcquired)
        return;

    GPU::SemaphorePtr acquire = deviceVulkan->RequestSemaphore();

    // acquire next image index
    VkResult res = vkAcquireNextImageKHR(
        deviceVulkan->device,
        swapchian->swapChain,
        0xFFFFFFFFFFFFFFFF,
        acquire->GetSemaphore(),
        VK_NULL_HANDLE,
        &swapchian->mImageIndex
    );
    if (res >= 0)
    {
        swapchainIndexHasAcquired = true;

        // acquire image to render
        acquire->Signal();

        // set swapchain acquire semaphore
        deviceVulkan->SetAcquireSemaphore(swapchian->mImageIndex, acquire);
    }
}

void WSI::EndFrame()
{
    deviceVulkan->EndFrameContext();

    // 检测在这一帧中是否使用过Swapchain
    if (!deviceVulkan->IsSwapchainTouched())
        return;

    swapchainIndexHasAcquired = false;

    // release在EndFrameContext中设置,确保image已经释放
    GPU::SemaphorePtr release = deviceVulkan->GetAndConsumeReleaseSemaphore();
    assert(release->IsSignalled());

    // present 
    VkResult result = VK_SUCCESS;
    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &release->GetSemaphore();
    info.swapchainCount = 1;
    info.pSwapchains = &swapchian->swapChain;
    info.pImageIndices = &swapchian->mImageIndex;
    info.pResults = &result;

    VkResult overall = vkQueuePresentKHR(deviceVulkan->GetPresentQueue(), &info);
    if (overall < 0)
    {
        release->WaitExternal();
    }
}

void WSI::SetPlatform(Platform* platform_)
{
	platform = platform_;
}

GPU::Swapchain* WSI::GetSwapChain()
{
    return swapchian;
}

GPU::DeviceVulkan* WSI::GetDevice()
{
    return deviceVulkan;
}
