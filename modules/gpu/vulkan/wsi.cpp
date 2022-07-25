#include "wsi.h"
#include "core\filesystem\filesystem.h"

namespace VulkanTest
{

WSI::WSI()
{
}

WSI::~WSI()
{
}

bool WSI::Initialize(GPU::SystemHandles& handles, U32 numThread)
{
    if (platform == nullptr)
    {
        Logger::Error("Invalid wsi platform.");
        return false;
    }

    // init vulkan context
    auto instanceExt = platform->GetRequiredExtensions(true);
    auto deviceExt = platform->GetRequiredDeviceExtensions();

    vulkanContext = CJING_NEW(GPU::VulkanContext)(numThread);
    vulkanContext->SetSystemHandles(handles);
    if (!vulkanContext->Initialize(instanceExt, deviceExt, true))
        return false;

    // init gpu
    deviceVulkan = CJING_NEW(GPU::DeviceVulkan);
    deviceVulkan->SetContext(*vulkanContext);

    // init surface
    surface = platform->CreateSurface(vulkanContext->GetInstance());
    if (surface == VK_NULL_HANDLE)
        return false;
  
    // need to check which queue family support surface
    VkBool32 supported = VK_FALSE;
    U32 queuePresentSupport = 0;
    for (U32 familyIndex : vulkanContext->GetQueueInfo().familyIndices)
    {
        if (familyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            auto ret = vkGetPhysicalDeviceSurfaceSupportKHR(vulkanContext->GetPhysicalDevice(), familyIndex, surface, &supported);
            if (ret == VK_SUCCESS && supported)
            {
                queuePresentSupport |= 1u << familyIndex;
            }
        }
    }

    if (!(queuePresentSupport & (1 << vulkanContext->GetQueueInfo().familyIndices[(U32)GPU::QUEUE_INDEX_GRAPHICS])))
        return false;

    // init swapchian
    if (!InitSwapchain(platform->GetWidth(), platform->GetHeight()))
    {
        Logger::Error("Failed to init swapchian.");
        return false;
    }

    isExternal = false;
	return true;
}

bool WSI::InitializeExternal(VkSurfaceKHR surface_, GPU::DeviceVulkan& device_, GPU::VulkanContext& context_, I32 width, I32 height)
{
    ASSERT(platform == nullptr);

    vulkanContext = &context_;
    deviceVulkan = &device_;
    surface = surface_;

    if (surface == VK_NULL_HANDLE)
        return false;

    // need to check which queue family support surface
    VkBool32 supported = VK_FALSE;
    U32 queuePresentSupport = 0;
    for (U32 familyIndex : vulkanContext->GetQueueInfo().familyIndices)
    {
        if (familyIndex != VK_QUEUE_FAMILY_IGNORED)
        {
            auto ret = vkGetPhysicalDeviceSurfaceSupportKHR(vulkanContext->GetPhysicalDevice(), familyIndex, surface, &supported);
            if (ret == VK_SUCCESS && supported)
            {
                queuePresentSupport |= 1u << familyIndex;
            }
        }
    }

    if (!(queuePresentSupport & (1 << vulkanContext->GetQueueInfo().familyIndices[(U32)GPU::QUEUE_INDEX_GRAPHICS])))
        return false;

    // init swapchian
    if (!InitSwapchain(width, height))
    {
        Logger::Error("Failed to init swapchian.");
        return false;
    }

    isExternal = true;
    return true;
}

void WSI::Uninitialize()
{
    if (vulkanContext != nullptr)
    {
        TeardownSwapchain();

        if (surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(vulkanContext->GetInstance(), surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    }

    if (!isExternal)
    {
        CJING_SAFE_DELETE(deviceVulkan);
        CJING_SAFE_DELETE(vulkanContext);
    }
}

void WSI::BeginFrame()
{
    deviceVulkan->NextFrameContext();
}

void WSI::EndFrame()
{
    deviceVulkan->EndFrameContext();
}

void WSI::PresentBegin()
{
    // Resize frame buffer
    if (platform != nullptr && (swapchain.swapchain == VK_NULL_HANDLE || platform->ShouldResize() || isSwapchinSuboptimal))
        UpdateFrameBuffer(platform->GetWidth(), platform->GetHeight());

    // 当前swapchian已经Acquired
    if (swapchainIndexHasAcquired)
        return;

    VkResult result;
    do
    {
        GPU::SemaphorePtr acquire = deviceVulkan->RequestSemaphore();

        // acquire next image index
        result = vkAcquireNextImageKHR(
            deviceVulkan->device,
            swapchain.swapchain,
            0xFFFFFFFFFFFFFFFF,
            acquire->GetSemaphore(),
            VK_NULL_HANDLE,
            &swapchain.swapchainImageIndex
        );

        if (result == VK_SUBOPTIMAL_KHR)
            isSwapchinSuboptimal = true;

        if (result >= 0)
        {
            swapchainIndexHasAcquired = true;
            acquire->Signal();

            // Set swapchain acquire semaphore and image index
            deviceVulkan->SetAcquireSemaphore(swapchain.swapchainImageIndex, acquire);
        }
        else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        {
            ASSERT(swapchain.swapchainWidth != 0);
            ASSERT(swapchain.swapchainHeight != 0);

            TeardownSwapchain();

            if (!InitSwapchain(swapchain.swapchainWidth, swapchain.swapchainHeight))
                return;
        }
        else
        {
            return;
        }

    } while (result < 0);
}

void WSI::PresentEnd()
{
    deviceVulkan->EndFrameContext();

    // 检测在这一帧中是否使用过Swapchain
    if (!deviceVulkan->IsSwapchainTouched())
        return;

    swapchainIndexHasAcquired = false;

    // release在EndFrameContext中设置,确保image已经释放
    GPU::SemaphorePtr release = deviceVulkan->GetAndConsumeReleaseSemaphore();
    assert(release);
    assert(release->IsSignalled());

    VkSemaphore releaseSemaphore = release->GetSemaphore();
    assert(releaseSemaphore != VK_NULL_HANDLE);

    // present 
    VkResult result = VK_SUCCESS;
    VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &releaseSemaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain.swapchain;
    info.pImageIndices = &swapchain.swapchainImageIndex;
    info.pResults = &result;

    VkResult overall = vkQueuePresentKHR(deviceVulkan->GetPresentQueue(), &info);
    if (overall == VK_SUBOPTIMAL_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        isSwapchinSuboptimal = true;
    }

    if (overall < 0 || result < 0)
    {
        Logger::Error("Failed to vkQueuePresentKHR");
        TeardownSwapchain();
        return;
    }
    else
    {
        // 暂时先不释放releaseSemaphore，直到Image再次等待
        release->WaitExternal();
        swapchain.releaseSemaphores[swapchain.swapchainImageIndex] = release;
    }
}

void WSI::SetPlatform(WSIPlatform* platform_)
{
	platform = platform_;
}

WSIPlatform* WSI::GetPlatform()
{
    return platform;
}

GPU::DeviceVulkan* WSI::GetDevice()
{
    return deviceVulkan;
}

GPU::VulkanContext* WSI::GetContext()
{
    return vulkanContext;
}

VkFormat WSI::GetSwapchainFormat() const
{
    return swapchain.swapchainFormat;
}

bool WSI::InitSwapchain(U32 width, U32 height)
{
    GPU::SwapChainDesc desc = {};
    desc.width = width;
    desc.height = height;
    desc.vsync = vsync;
    desc.bufferCount = 2;
    desc.fullscreen = false;

    U32 tryCounter = 0;
    GPU::SwapchainError err;
    do
    {
        err = deviceVulkan->CreateSwapchain(desc, surface, &swapchain);
        if (err != GPU::SwapchainError::None && platform != nullptr)
            platform->NotifySwapchainDimensions(0, 0);

        if (err == GPU::SwapchainError::NoSurface)
        {
            // Happendd when window is minimized
            Logger::Warning("WSI blocking because of minimization.");
            Logger::Warning("WSI woke up!");
            return false;
        }
        else if (err == GPU::SwapchainError::Error)
        {
            if (tryCounter++ > 3)
                return false;

            TeardownSwapchain();
        }
    } while (err != GPU::SwapchainError::None);

    return swapchain.swapchain != VK_NULL_HANDLE;
}

void WSI::UpdateFrameBuffer(U32 width, U32 height)
{
    if (vulkanContext == nullptr || deviceVulkan == nullptr)
        return;

    DrainSwapchain();

    if (!InitSwapchain(width, height))
        Logger::Error("Failed to init swapchian.");

    if (platform != nullptr)
        platform->NotifySwapchainDimensions(swapchain.swapchainWidth, swapchain.swapchainHeight);
}

void WSI::DrainSwapchain()
{
    swapchain.releaseSemaphores.clear();
    deviceVulkan->SetAcquireSemaphore(0, GPU::SemaphorePtr());
    deviceVulkan->GetAndConsumeReleaseSemaphore();
    deviceVulkan->WaitIdle();
}

void WSI::TeardownSwapchain()
{
    DrainSwapchain();

    swapchain.images.clear();
    swapchain.releaseSemaphores.clear();

    if (swapchain.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vulkanContext->GetDevice(), swapchain.swapchain, nullptr);
        swapchain.swapchain = VK_NULL_HANDLE;
    }
}

void WSIPlatform::OnDeviceCreated(GPU::DeviceVulkan* device) {}
void WSIPlatform::OnDeviceDestroyed() {}
void WSIPlatform::OnSwapchainCreated(GPU::DeviceVulkan* device, U32 width, U32 height, float aspectRatio, size_t numSwapchainImages, VkFormat format, VkSurfaceTransformFlagBitsKHR preRotate) {}
void WSIPlatform::OnSwapchainDestroyed() {}

}