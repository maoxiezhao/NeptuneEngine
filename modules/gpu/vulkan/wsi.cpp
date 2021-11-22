#include "wsi.h"
#include "gpu\vulkan\device.h"
#include "gpu\vulkan\context.h"

namespace {
#ifdef DEBUG
    static bool debugLayer = true;
#else
    static bool debugLayer = false;
#endif

    GPU::VulkanContext* vulkanContext = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    GPU::DeviceVulkan* deviceVulkan = nullptr;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchianImages;
    U32 swapchainWidth = 0;
    U32 swapchainHeight = 0;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    U32 swapchainImageIndex = 0;
    bool swapchainIndexHasAcquired = false;
}

bool WSI::Initialize()
{
    if (platform == nullptr)
    {
        Logger::Error("Invalid wsi platform.");
        return false;
    }

    // init vulkan context
    vulkanContext = new GPU::VulkanContext();

    auto instanceExt = platform->GetRequiredExtensions(true);
    auto deviceExt = platform->GetRequiredDeviceExtensions();
    if (!vulkanContext->Initialize(instanceExt, deviceExt, true))
        return false;

    // init gpu
    deviceVulkan = new GPU::DeviceVulkan();
    deviceVulkan->SetContext(*vulkanContext);

    // init surface
    surface = platform->CreateSurface(vulkanContext->GetInstance());
    if (surface == VK_NULL_HANDLE)
        return false;
  
    // need to check which queue family support surface
    VkBool32 supported = VK_FALSE;
    uint32_t queuePresentSupport = 0;
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

	return true;
}

void WSI::Uninitialize()
{
    TeardownSwapchain();

    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(vulkanContext->GetInstance(), surface, nullptr);

    delete deviceVulkan;
    delete vulkanContext;
}

void WSI::BeginFrame()
{
    deviceVulkan->NextFrameContext();

    if (swapchain == VK_NULL_HANDLE)
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
        swapchain,
        0xFFFFFFFFFFFFFFFF,
        acquire->GetSemaphore(),
        VK_NULL_HANDLE,
        &swapchainImageIndex
    );
    if (res >= 0)
    {
        swapchainIndexHasAcquired = true;

        // acquire image to render
        acquire->Signal();

        // set swapchain acquire semaphore and image index
        deviceVulkan->SetAcquireSemaphore(swapchainImageIndex, acquire);
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
    info.pSwapchains = &swapchain;
    info.pImageIndices = &swapchainImageIndex;
    info.pResults = &result;

    VkResult overall = vkQueuePresentKHR(deviceVulkan->GetPresentQueue(), &info);
    if (overall < 0)
    {
        release->WaitExternal();
    }
}

void WSI::SetPlatform(WSIPlatform* platform_)
{
	platform = platform_;
}

GPU::DeviceVulkan* WSI::GetDevice()
{
    return deviceVulkan;
}

bool WSI::InitSwapchain(uint32_t width, uint32_t height)
{
    if (surface == VK_NULL_HANDLE)
        return false;

    // get surface properties
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
    VkPhysicalDevice physicalDevice = vulkanContext->GetPhysicalDevice();
    bool useSurfaceInfo = false; // deviceVulkan->GetFeatures().supportsSurfaceCapabilities2;
    if (useSurfaceInfo)
    {
        surfaceInfo.surface = surface;
    
        VkSurfaceCapabilities2KHR surfaceCapabilities2 = { VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR };
        if (vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities2) != VK_SUCCESS)
            return false;

        surfaceProperties = surfaceCapabilities2.surfaceCapabilities;
    }
    else
    {
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceProperties) != VK_SUCCESS)
            return false;
    }

    // get surface formats
    uint32_t formatCount = 0;
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
    uint32_t presentModeCount = 0;
    std::vector<VkPresentModeKHR> presentModes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    }

    // find suitable surface format
    VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED };
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
        return false;
    }

    // find suitable present mode
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // 交换链是个队列，显示的时候从队列头拿一个图像，程序插入渲染的图像到队列尾。
                                                                      // 如果队列满了程序就要等待，这差不多像是垂直同步，显示刷新的时刻就是垂直空白
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
    uint32_t desiredSwapchainImages = 2;
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
        return false;

    swapchainWidth = swapchainSize.width;
    swapchainHeight = swapchainSize.height;
    swapchainFormat = surfaceFormat.format;

    // Get swapchian images
    uint32_t imageCount = 0;
    if (vkGetSwapchainImagesKHR(vulkanContext->GetDevice(), swapchain, &imageCount, nullptr) != VK_SUCCESS)
        return false;
    swapchianImages.resize(imageCount);
    if (vkGetSwapchainImagesKHR(vulkanContext->GetDevice(), swapchain, &imageCount, swapchianImages.data()) != VK_SUCCESS)
        return false;

    if (!deviceVulkan->InitSwapchain(swapchianImages, swapchainFormat, swapchainWidth, swapchainHeight))
        return false;

    return true;
}

void WSI::TeardownSwapchain()
{
    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(vulkanContext->GetDevice(), swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}
