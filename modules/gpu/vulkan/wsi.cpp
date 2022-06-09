#include "wsi.h"

namespace VulkanTest
{

WSI::WSI()
{
}

WSI::~WSI()
{
}

bool WSI::Initialize(U32 numThread)
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
    deviceVulkan->InitSwapchain(swapchianImages, swapchainFormat, swapchainWidth, swapchainHeight);

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
    deviceVulkan->InitSwapchain(swapchianImages, swapchainFormat, swapchainWidth, swapchainHeight);

    isExternal = true;
    return false;
}

void WSI::Uninitialize()
{
    if (vulkanContext != nullptr)
    {
        TeardownSwapchain();

        if (surface != VK_NULL_HANDLE)
            vkDestroySurfaceKHR(vulkanContext->GetInstance(), surface, nullptr);
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

    // Resize frame buffer
    if (swapchain == VK_NULL_HANDLE || platform->ShouldResize() || isSwapchinSuboptimal)
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
            swapchain,
            0xFFFFFFFFFFFFFFFF,
            acquire->GetSemaphore(),
            VK_NULL_HANDLE,
            &swapchainImageIndex
        );

        if (result == VK_SUBOPTIMAL_KHR)
            isSwapchinSuboptimal = true;

        if (result >= 0)
        {
            swapchainIndexHasAcquired = true;
            acquire->Signal();

            // Set swapchain acquire semaphore and image index
            deviceVulkan->SetAcquireSemaphore(swapchainImageIndex, acquire);
        }
        else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT)
        {
            ASSERT(swapchainWidth != 0);
            ASSERT(swapchainHeight != 0);

            TeardownSwapchain();

            if (!InitSwapchain(swapchainWidth, swapchainHeight))
                return;
            deviceVulkan->InitSwapchain(swapchianImages, swapchainFormat, swapchainWidth, swapchainHeight);
        }
        else
        {
            return;
        }

    } while (result < 0);
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
    info.pSwapchains = &swapchain;
    info.pImageIndices = &swapchainImageIndex;
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
        releaseSemaphores[swapchainImageIndex] = release;
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
    return swapchainFormat;
}

VkSurfaceKHR WSI::GetSurface()
{
    return surface;
}

bool WSI::InitSwapchain(U32 width, U32 height)
{
    U32 tryCounter = 0;
    SwapchainError err;
    do
    {
        err = InitSwapchainImpl(width, height);
        if (err != SwapchainError::None && platform != nullptr)
            platform->NotifySwapchainDimensions(0, 0);

        if (err == SwapchainError::NoSurface)
        {
            // Happendd when window is minimized
            Logger::Warning("WSI blocking because of minimization.");
            Logger::Warning("WSI woke up!");
            return false;
        }
        else if (err == SwapchainError::Error)
        {
            if (tryCounter++ > 3)
                return false;

            TeardownSwapchain();
        }
    } while (err != SwapchainError::None);

    return swapchain != VK_NULL_HANDLE;
}

WSI::SwapchainError WSI::InitSwapchainImpl(U32 width, U32 height)
{
    if (surface == VK_NULL_HANDLE)
    {
        Logger::Error("Failed to create swapchain with surface == VK_NULL_HANDLE");
        return SwapchainError::NoSurface;
    }

    VkPhysicalDevice physicalDevice = vulkanContext->GetPhysicalDevice();

    // Get surface properties
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
    bool useSurfaceInfo = deviceVulkan->GetFeatures().supportsSurfaceCapabilities2;
    useSurfaceInfo = false;
    if (useSurfaceInfo)
    {
        surfaceInfo.surface = surface;
    
        VkSurfaceCapabilities2KHR surfaceCapabilities2 = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };
        if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities2) != VK_SUCCESS)
            return SwapchainError::Error;

        surfaceProperties = surfaceCapabilities2.surfaceCapabilities;
    }
    else
    {
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceProperties) != VK_SUCCESS)
            return SwapchainError::Error;
    }

    // Check the window is minimized.
    if (surfaceProperties.maxImageExtent.width == 0 && surfaceProperties.maxImageExtent.height == 0)
        return SwapchainError::NoSurface;

    // Get surface formats
    U32 formatCount = 0;
    std::vector<VkSurfaceFormatKHR> formats;
    if (useSurfaceInfo)
    {
        vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatCount, nullptr);
        std::vector<VkSurfaceFormat2KHR> formats2(formatCount);
        for (auto& f : formats2)
        {
            f = {};
            f.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
        }
        vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatCount, formats2.data());
   
        formats.resize(formatCount);
        for (auto& f : formats2)
            formats.push_back(f.surfaceFormat);
    }
    else
    {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        }
    }


    // get present mode
    U32 presentModeCount = 0;
    std::vector<VkPresentModeKHR> presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    }

    // find suitable surface format
    surfaceFormat = { VK_FORMAT_UNDEFINED };
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    for (auto& format : formats)
    {
        if (!deviceVulkan->IsImageFormatSupported(format.format, features))
            continue;

        if (format.format == VkFormat::VK_FORMAT_R8G8B8A8_UNORM ||
            format.format == VkFormat::VK_FORMAT_B8G8R8A8_UNORM)
        {
            surfaceFormat = format;
            break;
        }
    }

    if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
    {
        Logger::Error("Failed to find suitable surface format.");
        return SwapchainError::Error;
    }

    // find suitable present mode
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; 
    bool isVsync = presentMode == PresentMode::SyncToVBlank;
    if (!isVsync)
    {
        bool allowMailbox = presentMode != PresentMode::UnlockedForceTearing;
        bool allowImmediate = presentMode != PresentMode::UnlockedNoTearing;

        for (auto& presentMode : presentModes)
        {
            if ((allowImmediate && presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) ||
                (allowMailbox && presentMode == VK_PRESENT_MODE_MAILBOX_KHR))
            {
                swapchainPresentMode = presentMode;
                break;
            }
        }
    }

    // Get desired swapchian images
    U32 desiredSwapchainImages = 2;
    if (desiredSwapchainImages < surfaceProperties.minImageCount)
        desiredSwapchainImages = surfaceProperties.minImageCount;
    if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
        desiredSwapchainImages = surfaceProperties.maxImageCount;

    // clamp the target width, height to boundaries.
    VkExtent2D swapchainSize;
    swapchainSize.width = std::max(std::min(width, surfaceProperties.maxImageExtent.width), surfaceProperties.minImageExtent.width);
    swapchainSize.height = std::max(std::min(height, surfaceProperties.maxImageExtent.height), surfaceProperties.minImageExtent.height);

    // create swapchain
    VkSwapchainKHR oldSwapchain = swapchain;
    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = desiredSwapchainImages;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchainSize;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = surfaceProperties.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = swapchainPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    if (vkCreateSwapchainKHR(vulkanContext->GetDevice(), &createInfo, nullptr, &swapchain) != VK_SUCCESS)
        return SwapchainError::Error;

    swapchainWidth = swapchainSize.width;
    swapchainHeight = swapchainSize.height;
    swapchainFormat = surfaceFormat.format;

    // Get swapchian images
    U32 imageCount = 0;
    if (vkGetSwapchainImagesKHR(vulkanContext->GetDevice(), swapchain, &imageCount, nullptr) != VK_SUCCESS)
        return SwapchainError::Error;

    swapchianImages.resize(imageCount);
    if (vkGetSwapchainImagesKHR(vulkanContext->GetDevice(), swapchain, &imageCount, swapchianImages.data()) != VK_SUCCESS)
        return SwapchainError::Error;

    releaseSemaphores.resize(imageCount);
    return SwapchainError::None;
}

void WSI::UpdateFrameBuffer(U32 width, U32 height)
{
    if (vulkanContext == nullptr || deviceVulkan == nullptr)
        return;

    DrainSwapchain();

    if (InitSwapchain(width, height))
    {
        deviceVulkan->InitSwapchain(swapchianImages, swapchainFormat, swapchainWidth, swapchainHeight);
    }

    if (platform != nullptr)
        platform->NotifySwapchainDimensions(swapchainWidth, swapchainHeight);
}

void WSI::DrainSwapchain()
{
    releaseSemaphores.clear();
    deviceVulkan->SetAcquireSemaphore(0, GPU::SemaphorePtr());
    deviceVulkan->GetAndConsumeReleaseSemaphore();
    deviceVulkan->WaitIdle();
}

void WSI::TeardownSwapchain()
{
    DrainSwapchain();

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vulkanContext->GetDevice(), swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void WSIPlatform::OnDeviceCreated(GPU::DeviceVulkan* device) {}
void WSIPlatform::OnDeviceDestroyed() {}
void WSIPlatform::OnSwapchainCreated(GPU::DeviceVulkan* device, U32 width, U32 height, float aspectRatio, size_t numSwapchainImages, VkFormat format, VkSurfaceTransformFlagBitsKHR preRotate) {}
void WSIPlatform::OnSwapchainDestroyed() {}

}