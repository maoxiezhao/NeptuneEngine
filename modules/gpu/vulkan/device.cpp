#include "device.h"
#include "math\hash.h"
#include "memory.h"
#include "TextureFormatLayout.h"
#include "core\platform\platform.h"
#include "core\platform\atomic.h"
#include "core\filesystem\filesystem.h"
#include "core\serialization\stream.h"

namespace VulkanTest
{
namespace GPU
{
namespace 
{
#ifdef VULKAN_MT

#define LOCK() std::lock_guard<std::mutex> holder_{mutex}
#define DRAIN_FRAME_LOCK() \
    std::unique_lock<std::mutex> holder_{ mutex }; \
    cond.wait(holder_, [&]() { \
        return frameCounter == 0; \
    })
#else
#define LOCK() ((void)0)
#define DRAIN_FRAME_LOCK()  ((void)0)
#endif

	static const QueueIndices QUEUE_FLUSH_ORDER[] = {
        QUEUE_INDEX_TRANSFER,
		QUEUE_INDEX_GRAPHICS,
		QUEUE_INDEX_COMPUTE
	};

#ifdef VULKAN_MT
    static std::atomic<U64> STATIC_COOKIE = 0;
#else
    static volatile U64 STATIC_COOKIE = 0;
#endif

    static inline VkImageViewType GetImageViewType(const ImageCreateInfo& createInfo, const ImageViewCreateInfo* view)
    {
        unsigned layers = view ? view->layers : createInfo.layers;
        unsigned base_layer = view ? view->baseLayer : 0;

        if (layers == VK_REMAINING_ARRAY_LAYERS)
            layers = createInfo.layers - base_layer;

        bool force_array =
            view ? (view->misc & IMAGE_VIEW_MISC_FORCE_ARRAY_BIT) : (createInfo.misc & IMAGE_MISC_FORCE_ARRAY_BIT);

        switch (createInfo.type)
        {
        case VK_IMAGE_TYPE_1D:
            assert(createInfo.width >= 1);
            assert(createInfo.height == 1);
            assert(createInfo.depth == 1);
            assert(createInfo.samples == VK_SAMPLE_COUNT_1_BIT);

            if (layers > 1 || force_array)
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            else
                return VK_IMAGE_VIEW_TYPE_1D;

        case VK_IMAGE_TYPE_2D:
            assert(createInfo.width >= 1);
            assert(createInfo.height >= 1);
            assert(createInfo.depth == 1);

            if ((createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) && (layers % 6) == 0)
            {
                assert(createInfo.width == createInfo.height);

                if (layers > 6 || force_array)
                    return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                else
                    return VK_IMAGE_VIEW_TYPE_CUBE;
            }
            else
            {
                if (layers > 1 || force_array)
                    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                else
                    return VK_IMAGE_VIEW_TYPE_2D;
            }

        case VK_IMAGE_TYPE_3D:
            assert(createInfo.width >= 1);
            assert(createInfo.height >= 1);
            assert(createInfo.depth >= 1);
            return VK_IMAGE_VIEW_TYPE_3D;

        default:
            assert(0 && "bogus");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    static QueueIndices GetQueueIndexFromQueueType(QueueType queueType)
    {
        return QueueIndices(queueType);
    }

    class ImageResourceCreator
    {
    public:
        DeviceVulkan& device;
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkImageView depthView = VK_NULL_HANDLE;
        VkImageView stencilView = VK_NULL_HANDLE;
        std::vector<VkImageView> rtViews;
        VkImageViewCreateInfo defaultViewCreateInfo = {};
        bool imageOwned = true;

    public:
        explicit ImageResourceCreator(DeviceVulkan& device_) :
            device(device_)
        {
        }

        explicit ImageResourceCreator(DeviceVulkan& device_, VkImage image_) :
            device(device_),
            image(image_)
        {
        }

        ~ImageResourceCreator()
        {
            if (imageOwned)
                Cleanup();
        }

        void Cleanup()
        {
            if (imageView)
                vkDestroyImageView(device.device, imageView, nullptr);
            if (depthView)
                vkDestroyImageView(device.device, depthView, nullptr);
            if (stencilView)
                vkDestroyImageView(device.device, stencilView, nullptr);
            for (auto& view : rtViews)
                vkDestroyImageView(device.device, view, nullptr);

            if (image != VK_NULL_HANDLE)
                vkDestroyImage(device.device, image, nullptr);
        }

        bool CreateDefaultView(const VkImageViewCreateInfo& viewInfo)
        {
            return vkCreateImageView(device.device, &viewInfo, nullptr, &imageView) == VK_SUCCESS;
        }

        bool CreateDepthAndStencilView(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo& viewInfo)
        {
            if (viewInfo.subresourceRange.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            {
                if ((createInfo.usage & ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
                {
                    auto tempInfo = viewInfo;
                    tempInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &depthView) != VK_SUCCESS)
                        return false;

                    tempInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &stencilView) != VK_SUCCESS)
                        return false;
                }
            }
            return true;
        }

        bool CreateRenderTargetView(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo& viewInfo)
        {
            if ((viewInfo.subresourceRange.layerCount > 1 || viewInfo.subresourceRange.levelCount > 1) &&
                createInfo.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
            {
                rtViews.reserve(viewInfo.subresourceRange.layerCount);

                VkImageViewCreateInfo tempInfo = viewInfo;
                tempInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                tempInfo.subresourceRange.baseMipLevel = viewInfo.subresourceRange.baseMipLevel;

                for (U32 layer = 0; layer < tempInfo.subresourceRange.layerCount; layer++)
                {
                    tempInfo.subresourceRange.levelCount = 1;
                    tempInfo.subresourceRange.layerCount = 1;
                    tempInfo.subresourceRange.baseArrayLayer = viewInfo.subresourceRange.baseArrayLayer + layer;

                    VkImageView rtView = VK_NULL_HANDLE;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &rtView) != VK_SUCCESS)
                        return false;

                    rtViews.push_back(rtView);
                }
            }

            return true;
        }

        void CreateDefaultViewCreateInfo(const ImageCreateInfo& imageCreateInfo)
        {
            defaultViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            defaultViewCreateInfo.image = image;
            defaultViewCreateInfo.viewType = GetImageViewType(imageCreateInfo, nullptr);
            defaultViewCreateInfo.format = imageCreateInfo.format;
            defaultViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.subresourceRange.aspectMask = formatToAspectMask(imageCreateInfo.format);
            defaultViewCreateInfo.subresourceRange.baseMipLevel = 0;
            defaultViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.levels;
            defaultViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            defaultViewCreateInfo.subresourceRange.layerCount = imageCreateInfo.layers;
        }

        bool CreateDefaultViews(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo* viewInfo)
        {
            if ((createInfo.usage & 
                (VK_IMAGE_USAGE_SAMPLED_BIT |
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) == 0)
            {
                Logger::Error("Cannot create image view unless certain usage flags are present.\n");
                return false;
            }

            if (viewInfo != nullptr)
                defaultViewCreateInfo = *viewInfo;
            else
                CreateDefaultViewCreateInfo(createInfo);
            
            if (!CreateDepthAndStencilView(createInfo, defaultViewCreateInfo))
                return false;

            if (!CreateRenderTargetView(createInfo, defaultViewCreateInfo))
                return false;

            if (!CreateDefaultView(defaultViewCreateInfo))
                return false;

            return true;
        }

        VkImageViewType GetDefaultImageViewType()const
        {
            return defaultViewCreateInfo.viewType;
        }
    };
}

DeviceVulkan::DeviceVulkan() : 
    shaderManager(*this),
    transientAllocator(*this),
    frameBufferAllocator(*this)
{
#ifdef VULKAN_MT
    STATIC_COOKIE.store(0);
#endif
}

DeviceVulkan::~DeviceVulkan()
{
    WaitIdle();

    wsi.Clear();

    if (pipelineCache!= VK_NULL_HANDLE)
    {
        FlushPipelineCache();
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
    }

    transientAllocator.Clear();
    frameBufferAllocator.Clear();

    DeinitTimelineSemaphores();
}

void DeviceVulkan::SetContext(VulkanContext& context)
{
    device = context.device;
    physicalDevice = context.physicalDevice;
    instance = context.instance;
    queueInfo = context.queueInfo;
    features = context.ext;
    systemHandles = context.GetSystemHandles();

    for (auto& index : queueInfo.familyIndices)
    {
        if (index != VK_QUEUE_FAMILY_IGNORED)
        {
            bool found = false;
            for (U32 i = 0; i < queueFamilies.size(); i++)
            {
                if (queueFamilies[i] == index)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                queueFamilies.push_back(index);
        }
    }

    // Init bindless descriptor set allocator
    InitBindless();

    // Init timeline semaphores
    InitTimelineSemaphores();

    // Init stock buffers
    InitStockSamplers();
    
    // Init pipelineCache
    InitPipelineCache();

    // Create frame resources
    InitFrameContext(GetBufferCount());

    // Init buffer pools
    vboPool.Init(this, 8 * 1024, 16, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 256);
    iboPool.Init(this, 4 * 1024, 16, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 256);
    uboPool.Init(this, 256 * 1024, std::max<VkDeviceSize>(16u, features.properties2.properties.limits.minUniformBufferOffsetAlignment), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 64);
    uboPool.SetSpillSize(VULKAN_MAX_UBO_SIZE);
    stagingPool.Init(this, 64 * 1024, std::max<VkDeviceSize>(16u, features.properties2.properties.limits.optimalBufferCopyOffsetAlignment), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 32);
    
    VkDeviceSize alignment = std::max<VkDeviceSize>(16u, features.properties2.properties.limits.minUniformBufferOffsetAlignment);
    alignment = std::max(alignment, features.properties2.properties.limits.minTexelBufferOffsetAlignment);
    storagePool.Init(
        this, 
        64 * 1024, 
        alignment, 
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, 
        32
    );
    storagePool.AllocateBindlessDescriptor(true);

    // Init managers
    memory.Initialize(this);
    fencePoolManager.Initialize(*this);
    semaphoreManager.Initialize(*this);
    eventManager.Initialize(*this);

    InitShaderManagerCache();
}

SwapchainError DeviceVulkan::CreateSwapchain(const SwapChainDesc& desc, VkSurfaceKHR surface, SwapChain* swapchain)
{
    ASSERT(swapchain != nullptr);

    if (surface == VK_NULL_HANDLE)
    {
        Logger::Error("Failed to create swapchain with surface == VK_NULL_HANDLE");
        return SwapchainError::NoSurface;
    }

    // Get surface properties
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR };
    bool useSurfaceInfo = features.supportsSurfaceCapabilities2;
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

    // clamp the target width, height to boundaries.
    VkExtent2D swapchainSize;
    if (surfaceProperties.currentExtent.width != 0xFFFFFFFF && surfaceProperties.currentExtent.height != 0xFFFFFFFF)
    {
        swapchainSize = surfaceProperties.currentExtent;
    }
    else
    {
        swapchainSize = { desc.width, desc.height };
        swapchainSize.width = std::max(surfaceProperties.minImageExtent.width, std::min(surfaceProperties.maxImageExtent.width, swapchainSize.width));
        swapchainSize.height = std::max(surfaceProperties.minImageExtent.height, std::min(surfaceProperties.maxImageExtent.height, swapchainSize.height));
    }

    // Check the window is minimized.
    //if (swapchainSize.width == 0 && swapchainSize.height == 0)
    //    return SwapchainError::NoSurface;

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
    VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED };
    VkSurfaceFormatKHR validFormat = { VK_FORMAT_UNDEFINED };
    VkFormatFeatureFlags features = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    bool valid = false;
    for (auto& format : formats)
    {
        if (!IsImageFormatSupported(format.format, features))
            continue;

        if (format.format == desc.format)
        {
            surfaceFormat = format;
            valid = true;
            break;
        }

        if (format.format == VkFormat::VK_FORMAT_R8G8B8A8_UNORM ||
            format.format == VkFormat::VK_FORMAT_B8G8R8A8_UNORM)
        {
            validFormat = format;
            break;
        }
    }

    if (!valid && validFormat.format != VK_FORMAT_UNDEFINED)
        surfaceFormat = validFormat;

    if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
    {
        Logger::Error("Failed to find suitable surface format.");
        return SwapchainError::Error;
    }

    // find suitable present mode
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    bool isVsync = desc.vsync;
    if (!isVsync)
    {
        for (auto& presentMode : presentModes)
        {
            if ((presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) ||
                (presentMode == VK_PRESENT_MODE_MAILBOX_KHR))
            {
                swapchainPresentMode = presentMode;
                break;
            }
        }
    }

    // Get desired swapchian images
    U32 desiredSwapchainImages = desc.bufferCount;
    if (desiredSwapchainImages < surfaceProperties.minImageCount)
        desiredSwapchainImages = surfaceProperties.minImageCount;
    if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
        desiredSwapchainImages = surfaceProperties.maxImageCount;

    // create swapchain
    VkSwapchainKHR oldSwapchain = swapchain->swapchain;
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

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain->swapchain) != VK_SUCCESS)
        return SwapchainError::Error;

    swapchain->swapchainWidth = swapchainSize.width;
    swapchain->swapchainHeight = swapchainSize.height;
    swapchain->swapchainFormat = surfaceFormat.format;

    // Get swapchian images
    U32 imageCount = 0;
    if (vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, nullptr) != VK_SUCCESS)
        return SwapchainError::Error;

    swapchain->vkImages.resize(imageCount);
    if (vkGetSwapchainImagesKHR(device, swapchain->swapchain, &imageCount, swapchain->vkImages.data()) != VK_SUCCESS)
        return SwapchainError::Error;

    swapchain->releaseSemaphores.resize(imageCount);
   
    // Create swapchain image views
    swapchain->images.clear();

    ImageCreateInfo imageCreateInfo = ImageCreateInfo::RenderTarget(desc.width, desc.height, desc.format);
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    for (int i = 0; i < swapchain->vkImages.size(); i++)
    {
        VkImageView imageView;
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain->vkImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = desc.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(device, &createInfo, nullptr, &imageView);
        assert(res == VK_SUCCESS);

        ImagePtr backbuffer = ImagePtr(imagePool.allocate(*this, swapchain->vkImages[i], imageView, DeviceAllocation(), imageCreateInfo, VK_IMAGE_VIEW_TYPE_2D));
        if (backbuffer)
        {
            backbuffer->SetInternalSyncObject();
            backbuffer->GetImageView().SetInternalSyncObject();
            backbuffer->DisownImge();
            backbuffer->SetSwapchainLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            swapchain->images.push_back(backbuffer);
            SetName(*backbuffer.get(), "Backbuffer");
        }
    }

    return SwapchainError::None;
}

void DeviceVulkan::InitFrameContext(U32 count)
{
    transientAllocator.Clear();
    frameBufferAllocator.Clear();
    frameResources.clear();

    for (U32 frameIndex = 0; frameIndex < count; frameIndex++)
    {
        auto frameResource = std::make_unique<FrameResource>(*this, frameIndex);
        frameResources.emplace_back(std::move(frameResource));
    }
}

IntrusivePtr<CommandList> DeviceVulkan::RequestCommandList(QueueType queueType)
{
    return RequestCommandListForThread(queueType);
}

IntrusivePtr<CommandList> DeviceVulkan::RequestCommandListForThread(QueueType queueType)
{
    LOCK();
    return RequestCommandListNolock(queueType);
}

IntrusivePtr<CommandList> DeviceVulkan::RequestCommandListNolock(QueueType queueType)
{
    QueueIndices queueIndex = GetQueueIndexFromQueueType(queueType);
    auto& pools = CurrentFrameResource().cmdPools[(int)queueIndex];
    auto& pool = pools.Get();
    if (pool == nullptr)
        pool = CJING_NEW(CommandPool)(this, queueInfo.familyIndices[queueIndex]);

    VkCommandBuffer cmd = pool->RequestCommandBuffer();
    if (cmd == VK_NULL_HANDLE)
        return IntrusivePtr<CommandList>();

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult res = vkBeginCommandBuffer(cmd, &info);
    assert(res == VK_SUCCESS);
    AddFrameCounter();

    IntrusivePtr<CommandList> cmdPtr(commandListPool.allocate(*this, cmd, queueType, pipelineCache));
    cmdPtr->SetThreadID(Platform::GetCurrentThreadID());

    // Set persistent storage block
    auto it = CurrentFrameResource().storageBlockMap.find(cmd);
    if (it != CurrentFrameResource().storageBlockMap.end())
        cmdPtr->SetStorageBlock(it->second);

    return cmdPtr;
}

QueueIndices DeviceVulkan::GetPhysicalQueueType(QueueType type) const
{
    return static_cast<QueueIndices>(type);
}

VkFormat DeviceVulkan::GetDefaultDepthStencilFormat() const
{
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkFormat DeviceVulkan::GetDefaultDepthFormat() const
{
    return VK_FORMAT_D32_SFLOAT;
}

RenderPassInfo DeviceVulkan::GetSwapchianRenderPassInfo(const SwapChain* swapchain, SwapchainRenderPassType swapchainRenderPassType)
{
    RenderPassInfo info = {};
    info.numColorAttachments = 1;
    info.colorAttachments[0] = &swapchain->images[wsi.index]->GetImageView();
    info.clearAttachments = ~0u;
    info.storeAttachments = 1u << 0;

    ImagePtr image = swapchain->images[wsi.index];
    if (swapchainRenderPassType == SwapchainRenderPassType::Depth)
    {
        info.opFlags |= RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
        auto att = RequestTransientAttachment(
            image->GetCreateInfo().width,
            image->GetCreateInfo().height,
            GetDefaultDepthFormat());
        info.depthStencil = &att->GetImageView();
    }
    else if (swapchainRenderPassType == SwapchainRenderPassType::DepthStencil)
    {
        info.opFlags |= RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
        auto att = RequestTransientAttachment(
            image->GetCreateInfo().width,
            image->GetCreateInfo().height,
            GetDefaultDepthStencilFormat());
        info.depthStencil = &att->GetImageView();
    }

    return info;
}

const char* GetShaderManagerCachePath()
{
    return ".export/shader_cache.bin";
}

void DeviceVulkan::InitShaderManagerCache()
{
    shaderManager.LoadShaderCache(GetShaderManagerCachePath());
}

RenderPass& DeviceVulkan::RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isCompatible)
{
    HashCombiner hash;
    VkFormat colorFormats[VULKAN_NUM_ATTACHMENTS];
    VkFormat depthStencilFormat = VkFormat::VK_FORMAT_UNDEFINED;  
    U32 lazy = 0;
    U32 optimal = 0;

    // Color attachments
    for (U32 i = 0; i < renderPassInfo.numColorAttachments; i++)
    {
        const ImageView& imageView = *renderPassInfo.colorAttachments[i];
        colorFormats[i] = imageView.GetInfo().format;

        if (imageView.GetImage()->GetCreateInfo().domain == ImageDomain::Transient)
            lazy |= 1u << i;
        if (imageView.GetImage()->GetLayoutType() == ImageLayoutType::Optimal)
            optimal |= 1u << i;

        hash.HashCombine(imageView.GetImage()->GetSwapchainLayout());
    }

    // Depth stencil
    if (renderPassInfo.depthStencil)
    {
        depthStencilFormat = renderPassInfo.depthStencil->GetInfo().format;
 
        if (renderPassInfo.depthStencil->GetImage()->GetCreateInfo().domain == ImageDomain::Transient)
            lazy |= 1u << renderPassInfo.numColorAttachments;
        if (renderPassInfo.depthStencil->GetImage()->GetLayoutType() == ImageLayoutType::Optimal)
            optimal |= 1u << renderPassInfo.numColorAttachments;
    }

    // Subpasses
    hash.HashCombine(renderPassInfo.numSubPasses);
    for (U32 i = 0; i < renderPassInfo.numSubPasses; i++)
    {
        auto& subpassInfo = renderPassInfo.subPasses[i];
        hash.HashCombine(subpassInfo.numColorAattachments);
        hash.HashCombine(subpassInfo.numInputAttachments);
        hash.HashCombine(subpassInfo.numResolveAttachments);
		for (U32 j = 0; j < subpassInfo.numColorAattachments; j++)
            hash.HashCombine(subpassInfo.colorAttachments[j]);
		for (U32 j = 0; j < subpassInfo.numInputAttachments; j++)
            hash.HashCombine(subpassInfo.inputAttachments[j]);
		for (U32 j = 0; j < subpassInfo.numResolveAttachments; j++)
            hash.HashCombine(subpassInfo.resolveAttachments[j]);
    }

    // Calculate hash
	for (U32 i = 0; i < renderPassInfo.numColorAttachments; i++)
        hash.HashCombine((U32)colorFormats[i]);

    hash.HashCombine(renderPassInfo.numColorAttachments);
    hash.HashCombine((U32)depthStencilFormat);

    // Compatible render passes do not care about load/store
    if (!isCompatible)
    {
        hash.HashCombine(renderPassInfo.clearAttachments);
        hash.HashCombine(renderPassInfo.loadAttachments);
        hash.HashCombine(renderPassInfo.storeAttachments);
        hash.HashCombine(optimal);
    }

    hash.HashCombine(lazy);

    LOCK();
    auto findIt = renderPasses.find(hash.Get());
    if (findIt != nullptr)
        return *findIt;

    RenderPass& renderPass = *renderPasses.emplace(hash.Get(), *this, renderPassInfo);
    renderPass.SetHash(hash.Get());
    return renderPass;
}

FrameBuffer& DeviceVulkan::RequestFrameBuffer(const RenderPassInfo& renderPassInfo)
{
    return frameBufferAllocator.RequestFrameBuffer(renderPassInfo);
}

PipelineLayout* DeviceVulkan::RequestPipelineLayout(const CombinedResourceLayout& resLayout)
{
    // Calculate hash for ShaderResLayout
    HashCombiner hash;
    hash.HashCombine(reinterpret_cast<const U32*>(resLayout.sets), sizeof(resLayout.sets));
    hash.HashCombine(&resLayout.stagesForBindings[0][0], sizeof(resLayout.stagesForBindings));
    hash.HashCombine(resLayout.pushConstantHash);
    hash.HashCombine(resLayout.attributeInputMask);
    hash.HashCombine(resLayout.renderTargetMask);
    
    // Immutable samplers
    for (unsigned set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
    {
        ForEachBit(resLayout.sets[set].immutableSamplerMask, [&](U32 bit) {
            ASSERT(bit < (U32)StockSampler::Count);
            hash.HashCombine(stockSamplers[bit]->GetHash());
        });
    }

    auto findIt = pipelineLayouts.find(hash.Get());
    if (findIt != nullptr)
        return findIt;

    return pipelineLayouts.emplace(hash.Get(), *this, resLayout);
}

SemaphorePtr DeviceVulkan::RequestSemaphore()
{
    VkSemaphore semaphore = semaphoreManager.Requset();
    return SemaphorePtr(semaphorePool.allocate(*this, semaphore, false));
}

SemaphorePtr DeviceVulkan::RequestEmptySemaphore()
{
    return SemaphorePtr(semaphorePool.allocate(*this));
}

EventPtr DeviceVulkan::RequestEvent()
{
    LOCK();
    VkEvent ent = eventManager.Requset();
    return EventPtr(eventPool.allocate(*this, ent));
}

EventPtr DeviceVulkan::RequestSignalEvent(VkPipelineStageFlags stages)
{
    VkEvent ent = eventManager.Requset();
    EventPtr ret = EventPtr(eventPool.allocate(*this, ent));
    ret->SetStages(stages);
    return ret;
}

Shader* DeviceVulkan::RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout)
{
    HashCombiner hash;
    hash.HashCombine(static_cast<const U32*>(pShaderBytecode), bytecodeLength);

    auto findIt = shaders.find(hash.Get());
    if (findIt != nullptr)
        return findIt;

    Shader* shader = shaders.emplace(hash.Get(), *this, stage, pShaderBytecode, bytecodeLength, layout);
    shader->SetHash(hash.Get());
    return shader;
}

Shader* DeviceVulkan::RequestShaderByHash(HashValue hash)
{
    return shaders.find(hash);
}

void DeviceVulkan::SetName(const Image& image, const char* name)
{
    if (features.supportDebugUtils)
    {
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_IMAGE;
        info.objectHandle = (uint64_t)image.GetImage();

        if (vkSetDebugUtilsObjectNameEXT)
            vkSetDebugUtilsObjectNameEXT(device, &info);
    }
}

void DeviceVulkan::SetName(const Buffer& buffer, const char* name)
{
    if (features.supportDebugUtils)
    {
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_BUFFER;
        info.objectHandle = (uint64_t)buffer.GetBuffer();

        if (vkSetDebugUtilsObjectNameEXT)
            vkSetDebugUtilsObjectNameEXT(device, &info);
    }
}

void DeviceVulkan::SetName(const Sampler& sampler, const char* name)
{
    if (features.supportDebugUtils)
    {
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_SAMPLER;
        info.objectHandle = (uint64_t)sampler.GetSampler();

        if (vkSetDebugUtilsObjectNameEXT)
            vkSetDebugUtilsObjectNameEXT(device, &info);
    }
}

ShaderProgram* DeviceVulkan::RequestProgram(const Shader* shaders[static_cast<U32>(ShaderStage::Count)])
{
    HashCombiner hasher;
    for (int i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
    {
        if (shaders[i] != nullptr)
            hasher.HashCombine(shaders[i]->GetHash());
    }

    auto findIt = programs.find(hasher.Get());
    if (findIt != nullptr)
        return findIt;

    ShaderProgramInfo info = {};
    for (int i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
    {
        if (shaders[i] != nullptr)
            info.shaders[i] = shaders[i];
    }

    ShaderProgram* program = programs.emplace(hasher.Get(), this, info);
    program->SetHash(hasher.Get());
    return program;
}

DescriptorSetAllocator& DeviceVulkan::RequestDescriptorSetAllocator(const DescriptorSetLayout& layout, const U32* stageForBinds)
{
	HashCombiner hasher;
    hasher.HashCombine(reinterpret_cast<const U32*>(&layout), sizeof(layout));
    hasher.HashCombine(stageForBinds, sizeof(U32) * VULKAN_NUM_BINDINGS);
    
	auto findIt = descriptorSetAllocators.find(hasher.Get());
	if (findIt != nullptr)
		return *findIt;

    DescriptorSetAllocator* allocator = descriptorSetAllocators.emplace(hasher.Get(), *this, layout, stageForBinds);
    allocator->SetHash(hasher.Get());
	return *allocator;
}

DescriptorSetAllocator* DeviceVulkan::GetBindlessDescriptorSetAllocator(BindlessReosurceType type)
{
    switch (type)
    {
    case BindlessReosurceType::SampledImage:
        return bindlessSampledImagesSetAllocator;
    case BindlessReosurceType::StorageImage:
        return bindlessStorageImagesSetAllocator;
    case BindlessReosurceType::StorageBuffer:
        return bindlessStorageBuffersSetAllocator;
    case BindlessReosurceType::Sampler:
        return bindlessSamplersSetAllocator;
    default:
        break;
    }
    return nullptr;
}

BindlessDescriptorPoolPtr DeviceVulkan::GetBindlessDescriptorPool(BindlessReosurceType type, U32 numSets, U32 numDescriptors)
{
    DescriptorSetAllocator* allocator = GetBindlessDescriptorSetAllocator(type);
    if (allocator == nullptr)
        return BindlessDescriptorPoolPtr();

    VkDescriptorPool pool = allocator->AllocateBindlessPool(numSets, numDescriptors);
    if (pool == VK_NULL_HANDLE)
        return BindlessDescriptorPoolPtr();

    return BindlessDescriptorPoolPtr(bindlessDescriptorPools.allocate(*this, pool, allocator, numSets, numDescriptors));
}

ImagePtr DeviceVulkan::RequestTransientAttachment(U32 w, U32 h, VkFormat format, U32 index, U32 samples, U32 layers)
{
    return transientAllocator.RequsetAttachment(w, h, format, index, samples, layers);
}

SamplerPtr DeviceVulkan::RequestSampler(const SamplerCreateInfo& createInfo, bool isImmutable)
{
    VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter = createInfo.magFilter;
    info.minFilter = createInfo.minFilter;
    info.mipmapMode = createInfo.mipmapMode;
    info.addressModeU = createInfo.addressModeU;
    info.addressModeV = createInfo.addressModeV;
    info.addressModeW = createInfo.addressModeW;
    info.mipLodBias = createInfo.mipLodBias;
    info.anisotropyEnable = createInfo.anisotropyEnable;
    info.maxAnisotropy = createInfo.maxAnisotropy;
    info.compareEnable = createInfo.compareEnable;
    info.compareOp = createInfo.compareOp;
    info.minLod = createInfo.minLod;
    info.maxLod = createInfo.maxLod;
    info.borderColor = createInfo.borderColor;
    info.unnormalizedCoordinates = createInfo.unnormalizedCoordinates;

    VkSampler sampler = VK_NULL_HANDLE;
    if (vkCreateSampler(device, &info, nullptr, &sampler) != VK_SUCCESS)
    {
        Logger::Error("Failed to create sampler.");
        return SamplerPtr();
    }

    return SamplerPtr(samplers.allocate(*this, sampler, createInfo, isImmutable));
}

ImmutableSampler* DeviceVulkan::RequestImmutableSampler(const SamplerCreateInfo& createInfo)
{
    HashCombiner hash;
    hash.HashCombine(createInfo.magFilter);
    hash.HashCombine(createInfo.minFilter);
    hash.HashCombine(createInfo.mipmapMode);
    hash.HashCombine(createInfo.addressModeU);
    hash.HashCombine(createInfo.addressModeV);
    hash.HashCombine(createInfo.addressModeW);
    hash.HashCombine(createInfo.mipLodBias);
    hash.HashCombine(createInfo.anisotropyEnable);
    hash.HashCombine(createInfo.maxAnisotropy);
    hash.HashCombine(createInfo.compareEnable);
    hash.HashCombine(createInfo.compareOp);
    hash.HashCombine(createInfo.minLod);
    hash.HashCombine(createInfo.maxLod);
    hash.HashCombine(createInfo.borderColor);
    hash.HashCombine(createInfo.unnormalizedCoordinates);

    auto findIt = immutableSamplers.find(hash.Get());
    if (findIt != nullptr)
        return findIt;

    ImmutableSampler* sampler = immutableSamplers.emplace(hash.Get(), *this, createInfo);
    sampler->SetHash(hash.Get());
    return sampler;
}

void DeviceVulkan::RequestVertexBufferBlock(BufferBlock& block, VkDeviceSize size)
{
    LOCK();
    RequestVertexBufferBlockNolock(block, size);
}

void DeviceVulkan::RequestVertexBufferBlockNolock(BufferBlock& block, VkDeviceSize size)
{
    RequestBufferBlock(block, size, vboPool, CurrentFrameResource().vboBlocks, &pendingBufferBlocks.vbo);
}

void DeviceVulkan::RequestIndexBufferBlock(BufferBlock& block, VkDeviceSize size)
{
    LOCK();
    RequestIndexBufferBlockNoLock(block, size);
}

void DeviceVulkan::RequestIndexBufferBlockNoLock(BufferBlock& block, VkDeviceSize size)
{
    RequestBufferBlock(block, size, iboPool, CurrentFrameResource().iboBlocks, &pendingBufferBlocks.ibo);
}

void DeviceVulkan::RequestUniformBufferBlock(BufferBlock& block, VkDeviceSize size)
{
    LOCK();
    RequestUniformBufferBlockNoLock(block, size);
}

void DeviceVulkan::RequestUniformBufferBlockNoLock(BufferBlock& block, VkDeviceSize size)
{
    RequestBufferBlock(block, size, uboPool, CurrentFrameResource().uboBlocks, &pendingBufferBlocks.ubo);
}

void DeviceVulkan::RequestStagingBufferBlock(BufferBlock& block, VkDeviceSize size)
{
    LOCK();
    RequestStagingBufferBlockNolock(block, size);
}

void DeviceVulkan::RequestStagingBufferBlockNolock(BufferBlock& block, VkDeviceSize size)
{
    RequestBufferBlock(block, size, stagingPool, CurrentFrameResource().stagingBlocks, nullptr);
}

void DeviceVulkan::RequestStorageBufferBlock(BufferBlock& block, VkDeviceSize size)
{
    LOCK();
    RequestStorageBufferBlockNolock(block, size);
}

void DeviceVulkan::RequestStorageBufferBlockNolock(BufferBlock& block, VkDeviceSize size)
{
    RequestBufferBlock(block, size, storagePool, CurrentFrameResource().storageBlocks, nullptr);
}

void DeviceVulkan::RecordStorageBufferBlock(BufferBlock& block, CommandList& cmd)
{
    CurrentFrameResource().storageBlockMap[cmd.GetCommandBuffer()] = block;
}

void DeviceVulkan::RequestBufferBlock(BufferBlock& block, VkDeviceSize size, BufferPool& pool, std::vector<BufferBlock>& recycle, std::vector<BufferBlock>* pending)
{
    if (block.mapped != nullptr)
        UnmapBuffer(*block.cpu, MemoryAccessFlag::MEMORY_ACCESS_WRITE_BIT);

    if (block.offset == 0)
    {
        if (block.capacity == pool.GetBlockSize())
            pool.RecycleBlock(block);
    }
    else
    {
        if (block.cpu != block.gpu )
        {
            // Pending this block, is will copy from gpu to cpu before submit
            ASSERT(pending != nullptr);
            pending->push_back(block);
        }

        if (block.capacity == pool.GetBlockSize())
            recycle.push_back(block);
    }

    if (size > 0)
        block = pool.RequestBlock(size);
    else
        block = {};
}

ImagePtr DeviceVulkan::CreateImage(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData)
{
    if (pInitialData != nullptr)
    {
        InitialImageBuffer stagingBuffer = CreateImageStagingBuffer(createInfo, pInitialData);
        return CreateImageFromStagingBuffer(createInfo, &stagingBuffer);
    }   
    else
    {
        return CreateImageFromStagingBuffer(createInfo, nullptr);
    }
}

InitialImageBuffer DeviceVulkan::CreateImageStagingBuffer(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData)
{
    // Setup texture format layout
    TextureFormatLayout layout;
    U32 copyLevels = createInfo.levels;
    switch (createInfo.type)
    {
    case VK_IMAGE_TYPE_1D:
        layout.SetTexture1D(createInfo.format, createInfo.width, createInfo.layers, copyLevels);
        break;
    case VK_IMAGE_TYPE_2D:
        layout.SetTexture2D(createInfo.format, createInfo.width, createInfo.height, createInfo.layers, copyLevels);
        break;
    case VK_IMAGE_TYPE_3D:
        layout.SetTexture3D(createInfo.format, createInfo.width, createInfo.height, createInfo.depth, copyLevels);
        break;
    default:
        return {};
    }

    // Create staging buffer
    BufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.domain = BufferDomain::Host;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.size = layout.GetRequiredSize();

    InitialImageBuffer imageBuffer = {};
    imageBuffer.buffer = CreateBuffer(bufferCreateInfo, nullptr);
    if (!imageBuffer.buffer)
        return InitialImageBuffer();
    SetName(*imageBuffer.buffer, "image_upload_staging_buffer");

    // Map staging buffer and copy initial datas to staging buffer
    U8* mapped = static_cast<U8*>(MapBuffer(*imageBuffer.buffer, MEMORY_ACCESS_WRITE_BIT));
    if (mapped != nullptr)
    {
        layout.SetBuffer(mapped, layout.GetRequiredSize());

        for (U32 level = 0, index = 0; level < copyLevels; level++, index++)
        {
            // Calculate dst stride
            const auto& mipInfo = layout.GetMipInfo(level);
            U32 dstRowSize = layout.GetRowSize(level);
            U32 dstHeightStride = layout.GetLayerSize(level);

            for (U32 layer = 0; layer < createInfo.layers; layer++)
            {
                U32 srcRowLength = pInitialData[index].rowLength ? pInitialData[index].rowLength : mipInfo.rowLength;
                U32 srcImageHeight = pInitialData[index].imageHeight ? pInitialData[index].imageHeight : mipInfo.imageHeight;
                U32 srcRowStride = (U32)layout.RowByteStride(srcRowLength);
                U32 srcHeightStride = (U32)layout.LayerByteStride(srcImageHeight, srcRowStride);

                U8* dst = static_cast<uint8_t*>(layout.Data(layer, level));
                const U8* src = static_cast<const uint8_t*>(pInitialData[index].data);

                for (U32 depth = 0; depth < mipInfo.depth; depth++)
                    for (U32 y = 0; y < mipInfo.blockH; y++)
                        memcpy(dst + depth * dstHeightStride + y * dstRowSize, src + depth * srcHeightStride + y * srcRowStride, dstRowSize);
            }
        }
        UnmapBuffer(*imageBuffer.buffer, MEMORY_ACCESS_WRITE_BIT);

        // Create VkBufferImageCopies
        layout.BuildBufferImageCopies(imageBuffer.numBlits, imageBuffer.blits);
    }
    return imageBuffer;
}

ImagePtr DeviceVulkan::CreateImageFromStagingBuffer(const ImageCreateInfo& createInfo, const InitialImageBuffer* stagingBuffer)
{
    VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.format = createInfo.format;
    info.extent.width = createInfo.width;
    info.extent.height = createInfo.height;
    info.extent.depth = createInfo.depth;
    info.imageType = createInfo.type;
    info.mipLevels = createInfo.levels;
    info.arrayLayers = createInfo.layers;
    info.samples = createInfo.samples;
    info.usage = createInfo.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.flags = createInfo.flags;

    if (createInfo.domain == ImageDomain::LinearHostCached || createInfo.domain == ImageDomain::LinearHost)
    {
        info.tiling = VK_IMAGE_TILING_LINEAR;
        info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }
    else
    {
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if (createInfo.domain == ImageDomain::Transient)
        info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    if (stagingBuffer != nullptr)
        info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Check is concurrent queue
    U32 queueFlags = createInfo.misc &
        (IMAGE_MISC_CONCURRENT_QUEUE_GRAPHICS_BIT |
         IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_COMPUTE_BIT |
         IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_GRAPHICS_BIT |
         IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_TRANSFER_BIT);
    bool concurrentQueue = queueFlags != 0;
    if (concurrentQueue && queueFamilies.size() > 1)
    {
        info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = (uint32_t)queueFamilies.size();
        info.pQueueFamilyIndices = queueFamilies.data();
    }
    else
    {
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
    }

    if (info.tiling == VK_IMAGE_TILING_LINEAR)
    {
        if (stagingBuffer)
            return ImagePtr();

        if (info.mipLevels > 1)
            return ImagePtr();
        if (info.arrayLayers > 1)
            return ImagePtr();
        if (info.imageType != VK_IMAGE_TYPE_2D)
            return ImagePtr();
        if (info.samples != VK_SAMPLE_COUNT_1_BIT)
            return ImagePtr();
    }

    // Create VKImage by allocator
    VkImage image = VK_NULL_HANDLE;
    DeviceAllocation allocation;
    if (!memory.CreateImage(info, createInfo.domain, image, &allocation))
    {
        Logger::Warning("Failed to create image");
        return ImagePtr(nullptr);
    }
    
    // Create image views
    bool hasView = (info.usage & (
        VK_IMAGE_USAGE_SAMPLED_BIT | 
        VK_IMAGE_USAGE_STORAGE_BIT | 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) != 0 &&
        (createInfo.misc & IMAGE_MISC_NO_DEFAULT_VIEWS_BIT) == 0;

    ImageResourceCreator viewCreator(*this, image);
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    if (hasView)
    {
        if (!viewCreator.CreateDefaultViews(createInfo, nullptr))
            return ImagePtr();

        viewType = viewCreator.GetDefaultImageViewType();
    }

    // Create image ptr
    ImagePtr imagePtr(imagePool.allocate(*this, image, viewCreator.imageView, allocation, createInfo, viewType));
    if (!imagePtr)
        return imagePtr;

    viewCreator.imageOwned = false;

    if (hasView)
    {
        auto& imageView = imagePtr->GetImageView();
        imageView.SetDepthStencilView(viewCreator.depthView, viewCreator.stencilView);
        imageView.SetRenderTargetViews(std::move(viewCreator.rtViews));
    }

    imagePtr->stageFlags = Image::ConvertUsageToPossibleStages(createInfo.usage);
    imagePtr->accessFlags = Image::ConvertUsageToPossibleAccess(createInfo.usage);

    // If staging buffer is not null, create transition_cmd and copy the staging buffer to gpu buffer
    CommandListPtr transitionCmd;
    if (stagingBuffer != nullptr)
    {
        ASSERT(createInfo.domain != ImageDomain::Transient);
        ASSERT(createInfo.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED);

        CommandListPtr graphicsCmd = RequestCommandList(QueueType::QUEUE_TYPE_GRAPHICS);

        // Use the TRANSFER queue to copy data over to the GPU
        CommandListPtr transferCmd;
        if (queueInfo.queues[QUEUE_INDEX_GRAPHICS] != queueInfo.queues[QUEUE_INDEX_TRANSFER])
            transferCmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_TRANSFER);
        else
            transferCmd = graphicsCmd; 

        // Add image barrier until finish transfer
        transferCmd->ImageBarrier(
            *imagePtr, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT
        );

        transferCmd->BeginEvent("copy_image_to_gpu");
        transferCmd->CopyToImage(*imagePtr, *stagingBuffer->buffer, stagingBuffer->numBlits, stagingBuffer->blits.data());
        transferCmd->EndEvent();

        if (queueInfo.queues[QUEUE_INDEX_GRAPHICS] != queueInfo.queues[QUEUE_INDEX_TRANSFER])
        {
           // If queue is not concurent, we will also need a release + acquire barrier 
           // to marshal ownership from transfer queue over to graphics
            if (!concurrentQueue && queueInfo.familyIndices[QUEUE_INDEX_GRAPHICS] != queueInfo.familyIndices[QUEUE_INDEX_TRANSFER])
            {
                VkImageMemoryBarrier release = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                release.image = imagePtr->GetImage();
                release.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                release.dstAccessMask = 0;
                release.srcQueueFamilyIndex = queueInfo.familyIndices[QUEUE_INDEX_TRANSFER];
                release.dstQueueFamilyIndex = queueInfo.familyIndices[QUEUE_INDEX_GRAPHICS];
                release.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                release.newLayout = createInfo.initialLayout;
                release.subresourceRange.levelCount = info.mipLevels;
                release.subresourceRange.aspectMask = formatToAspectMask(info.format);
                release.subresourceRange.layerCount = info.arrayLayers;

                VkImageMemoryBarrier acquire = release;
                acquire.srcAccessMask = 0;
                acquire.dstAccessMask = imagePtr->GetAccessFlags() & Image::ConvertLayoutToPossibleAccess(createInfo.initialLayout);
            
                transferCmd->Barrier(
                    VK_PIPELINE_STAGE_TRANSFER_BIT, 
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
                    0, nullptr,
                    1, &release);
                
                graphicsCmd->Barrier(
                    imagePtr->GetStageFlags(), 
                    imagePtr->GetStageFlags(), 
                    0, nullptr,
                    1, &acquire);
            }

            // Submit transfer cmd
            SemaphorePtr semaphore;
            Submit(transferCmd, nullptr, 1, &semaphore);

            // Graphics queue waits for transition finish
            AddWaitSemaphore(QueueIndices::QUEUE_INDEX_GRAPHICS, semaphore, imagePtr->GetStageFlags(), true);
        
            transitionCmd = std::move(graphicsCmd);
        }
    }
    else if (createInfo.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
    {
        // Barrier for the layout transition
        auto cmd = RequestCommandList(QueueType::QUEUE_TYPE_GRAPHICS);
        cmd->ImageBarrier(
            *imagePtr,
            info.initialLayout,
            createInfo.initialLayout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
            imagePtr->GetStageFlags(), 
            imagePtr->GetAccessFlags()& Image::ConvertLayoutToPossibleAccess(createInfo.initialLayout)
        );
        transitionCmd = std::move(cmd);
    }

    if (transitionCmd)
    {
        LOCK();
        SubmitNolock(transitionCmd, nullptr, 0, nullptr);
        if (concurrentQueue)
            FlushFrame(QueueIndices::QUEUE_INDEX_GRAPHICS);
    }

    return imagePtr;
}

ImageViewPtr DeviceVulkan::CreateImageView(const ImageViewCreateInfo& viewInfo)
{
    auto& imageCreateInfo = viewInfo.image->GetCreateInfo();
    VkFormat format = viewInfo.format != VK_FORMAT_UNDEFINED ? viewInfo.format : imageCreateInfo.format;
    VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    createInfo.image = viewInfo.image->GetImage();
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components = viewInfo.swizzle;
    createInfo.subresourceRange.aspectMask = formatToAspectMask(format);
    createInfo.subresourceRange.baseMipLevel = viewInfo.baseLevel;
    createInfo.subresourceRange.levelCount = viewInfo.levels;
    createInfo.subresourceRange.baseArrayLayer = viewInfo.baseLayer;
    createInfo.subresourceRange.layerCount = viewInfo.layers;

    if (viewInfo.viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
        createInfo.viewType = GetImageViewType(imageCreateInfo, &viewInfo);
    else
        createInfo.viewType = viewInfo.viewType;

    ImageResourceCreator creator(*this);
    if (!creator.CreateDefaultViews(imageCreateInfo, &createInfo))
        return ImageViewPtr();

    ImageViewPtr viewPtr(imageViews.allocate(*this, creator.imageView, viewInfo));
    if (viewPtr)
    {
        return viewPtr;
    }

    return ImageViewPtr();
}

BufferPtr DeviceVulkan::CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData)
{
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = createInfo.size;
    info.usage = createInfo.usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.flags = 0;

    if (features.features_1_2.bufferDeviceAddress == VK_TRUE)
        info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    if (queueFamilies.size() > 1)
    {
        info.sharingMode = VK_SHARING_MODE_CONCURRENT;
        info.queueFamilyIndexCount = (uint32_t)queueFamilies.size();
        info.pQueueFamilyIndices = queueFamilies.data();
    }
    else
    {
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.queueFamilyIndexCount = 0;
        info.pQueueFamilyIndices = nullptr;
    }

    VkBuffer buffer;
    DeviceAllocation allocation;
    if (!memory.CreateBuffer(info, createInfo.domain, buffer, &allocation))
    {
        Logger::Warning("Failed to create buffer");
        return BufferPtr(nullptr);
    }

    auto tmpinfo = createInfo;
    tmpinfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    BufferPtr bufferPtr(buffers.allocate(*this, buffer, allocation, tmpinfo));
    if (!bufferPtr)
    {
        Logger::Warning("Failed to create buffer");
        return BufferPtr(nullptr);
    }
     
    bool needInitialize = (createInfo.misc & BUFFER_MISC_ZERO_INITIALIZE_BIT) != 0 || (initialData != nullptr);
    if (createInfo.domain == BufferDomain::Device && needInitialize && !allocation.IsHostVisible())
    {
        // Buffer is device only, create a staging buffer and copy it
        CommandListPtr cmd;
        if (initialData != nullptr)
        {
            // Create a staging buffer which domain is host
            BufferCreateInfo stagingCreateInfo = createInfo;
            stagingCreateInfo.domain = BufferDomain::Host;
            BufferPtr stagingBuffer = CreateBuffer(stagingCreateInfo, initialData);
            SetName(*stagingBuffer, "Buffer_upload_staging_buffer");

            // Request async transfer cmd to copy staging buffer
            cmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_TRANSFER);
            cmd->BeginEvent("Fill_buffer_staging");
            cmd->CopyBuffer(*bufferPtr, *stagingBuffer);
            cmd->EndEvent();
        }
        else
        {
            cmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_COMPUTE);
            cmd->BeginEvent("Fill_buffer_staging");
            cmd->FillBuffer(bufferPtr, 0);
            cmd->EndEvent();
        }

        if (cmd)
        {
            LOCK();
            SubmitStaging(cmd, info.usage, true);
        }
    }
    else if (needInitialize)
    {
        void* ptr = memory.MapMemory(allocation, MemoryAccessFlag::MEMORY_ACCESS_WRITE_BIT, 0, allocation.size);
        if (ptr == nullptr)
            return BufferPtr(nullptr);

        if (initialData != nullptr)
            memcpy(ptr, initialData, createInfo.size);
        else
            memset(ptr, 0, createInfo.size);

        memory.UnmapMemory(allocation, MemoryAccessFlag::MEMORY_ACCESS_WRITE_BIT, 0, allocation.size);
    }
    return bufferPtr;
}

BufferViewPtr DeviceVulkan::CreateBufferView(const BufferViewCreateInfo& viewInfo)
{
    VkBufferViewCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
    info.buffer = viewInfo.buffer->GetBuffer();
    info.format = viewInfo.format;
    info.offset = viewInfo.offset;
    info.range = viewInfo.range;

    VkBufferView view;
    if (vkCreateBufferView(device, &info, nullptr, &view) != VK_SUCCESS)
        return BufferViewPtr();

    return BufferViewPtr(bufferViews.allocate(*this, view, viewInfo));
}

DeviceAllocationOwnerPtr DeviceVulkan::AllocateMemmory(const MemoryAllocateInfo& allocInfo)
{
    DeviceAllocation alloc = {};
    if (!memory.Allocate(
        (U32)allocInfo.requirements.size,
        (U32)allocInfo.requirements.alignment,
        allocInfo.requirements.memoryTypeBits,
        allocInfo.usage,
        &alloc))
        return DeviceAllocationOwnerPtr();

    return DeviceAllocationOwnerPtr(allocations.allocate(*this, alloc));
}

BindlessDescriptorPtr DeviceVulkan::CreateBindlessStroageBuffer(const Buffer& buffer, VkDeviceSize offset, VkDeviceSize range)
{
    auto heap = GetBindlessDescriptorHeap(BindlessReosurceType::StorageBuffer);
    if (heap == nullptr || !heap->IsInitialized())
        return BindlessDescriptorPtr();

    I32 index = heap->Allocate();
    if (index < 0)
        return BindlessDescriptorPtr();

    heap->GetPool().SetBuffer(index, buffer.GetBuffer(), offset, range);
    return BindlessDescriptorPtr(bindlessDescriptorHandlers.allocate(*this, BindlessReosurceType::StorageBuffer, index));
}

BindlessDescriptorPtr DeviceVulkan::CreateBindlessSampledImage(const ImageView& imageView, VkImageLayout imageLayout)
{
    auto heap = GetBindlessDescriptorHeap(BindlessReosurceType::SampledImage);
    if (heap == nullptr || !heap->IsInitialized())
        return BindlessDescriptorPtr();

    I32 index = heap->Allocate();
    if (index < 0)
        return BindlessDescriptorPtr();

    heap->GetPool().SetTexture(index, imageView, imageLayout);
    return BindlessDescriptorPtr(bindlessDescriptorHandlers.allocate(*this, BindlessReosurceType::SampledImage, index));
}

void DeviceVulkan::NextFrameContext()
{
    DRAIN_FRAME_LOCK();

    // submit remain queue
    EndFrameContextNolock();

    transientAllocator.BeginFrame();
    frameBufferAllocator.BeginFrame();

#ifdef VULKAN_MT
    // descriptor set begin
    for (auto& set : descriptorSetAllocators.GetReadOnly())
        set.BeginFrame();
    for (auto& set : descriptorSetAllocators.GetReadWrite())
        set.BeginFrame();
#else
    // descriptor set begin
    for (auto& kvp : descriptorSetAllocators)
        kvp.second->BeginFrame();
#endif

    // update frameIndex
    FRAMECOUNT++;
    frameIndex++;
    if (frameIndex >= frameResources.size())
        frameIndex = 0;

    // begin frame resources
    CurrentFrameResource().Begin();

    isRendering = true;
}

void DeviceVulkan::EndFrameContext()
{
    DRAIN_FRAME_LOCK();
    EndFrameContextNolock();

    isRendering = false;
}

void DeviceVulkan::EndFrameContextNolock()
{
    InternalFence fence;
    auto& curFrame = CurrentFrameResource();
    for (auto& queueIndex : QUEUE_FLUSH_ORDER)
    {
        if (queueDatas[queueIndex].needFence || !curFrame.submissions[queueIndex].empty())
        {
            SubmitQueue(queueIndex, &fence, 0, nullptr);
            queueDatas[queueIndex].needFence = false;
        }
    }
}

void DeviceVulkan::FlushFrames()
{
    LOCK();
    for (auto& queue : QUEUE_FLUSH_ORDER)
        FlushFrame(queue);
}

void DeviceVulkan::FlushFrame(QueueIndices queueIndex)
{
    if (queueInfo.queues[queueIndex] == VK_NULL_HANDLE)
        return;

    if (queueIndex == QueueIndices::QUEUE_INDEX_TRANSFER)
        SyncPendingBufferBlocks();

    SubmitQueue(queueIndex, nullptr, 0, nullptr);
}

void DeviceVulkan::Submit(CommandListPtr& cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore)
{
    LOCK();
    SubmitNolock(std::move(cmd), fence, semaphoreCount, semaphore);
}

void DeviceVulkan::SetAcquireSemaphore(U32 index, SemaphorePtr acquire)
{
    wsi.acquire = std::move(acquire);
    if (wsi.acquire)
        wsi.acquire->SetInternalSyncObject();

    wsi.index = index;
    wsi.consumed = false;
}

void DeviceVulkan::AddWaitSemaphore(QueueType queueType, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush)
{
    LOCK();
    AddWaitSemaphoreNolock(GetPhysicalQueueType(queueType), semaphore, stages, flush);
}

void DeviceVulkan::AddWaitSemaphore(QueueIndices queueIndex, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush)
{
    LOCK();
    AddWaitSemaphoreNolock(queueIndex, semaphore, stages, flush);
}

void DeviceVulkan::AddWaitSemaphoreNolock(QueueIndices queueIndex, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush)
{
    if (flush)
        FlushFrame(queueIndex);

    semaphore->SignalPendingWait();
    auto& queueData = queueDatas[(int)queueIndex];
    queueData.waitSemaphores.push_back(semaphore);
    queueData.waitStages.push_back(stages);
    queueData.needFence = true;
}

void DeviceVulkan::ReleaseFrameBuffer(VkFramebuffer buffer)
{
    LOCK();
    CurrentFrameResource().destroyedFrameBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseImage(VkImage image)
{
    LOCK();
    CurrentFrameResource().destroyedImages.push_back(image);
}

void DeviceVulkan::ReleaseImageView(VkImageView imageView)
{
    LOCK();
    CurrentFrameResource().destroyedImageViews.push_back(imageView);
}

void DeviceVulkan::ReleaseFence(VkFence fence, bool isWait)
{
    LOCK();
    if (isWait)
    {
        vkResetFences(device, 1, &fence);
        fencePoolManager.Recyle(fence);
    }
    else
    {
        CurrentFrameResource().recyleFences.push_back(fence);
    }
}

void DeviceVulkan::ReleaseBuffer(VkBuffer buffer)
{
    LOCK();
    CurrentFrameResource().destroyedBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseBufferView(VkBufferView bufferView)
{
    LOCK();
    CurrentFrameResource().destroyedBufferViews.push_back(bufferView);
}

void DeviceVulkan::ReleaseSampler(VkSampler sampler)
{
    LOCK();
    CurrentFrameResource().destroyedSamplers.push_back(sampler);
}

void DeviceVulkan::ReleaseDescriptorPool(VkDescriptorPool pool)
{
    LOCK();
    CurrentFrameResource().destroyedDescriptorPool.push_back(pool);
}

void DeviceVulkan::ReleasePipeline(VkPipeline pipeline)
{
    LOCK();
    CurrentFrameResource().destroyedPipelines.push_back(pipeline);
}

void DeviceVulkan::FreeMemory(const DeviceAllocation& allocation)
{
    LOCK();
    CurrentFrameResource().destroyedAllocations.push_back(allocation);
}

void DeviceVulkan::ReleaseBindlessResource(I32 index, BindlessReosurceType type)
{
    LOCK();
    CurrentFrameResource().destroyedBindlessResources.push_back({ index, type });
}

void DeviceVulkan::ReleaseSemaphore(VkSemaphore semaphore)
{
    LOCK();
    CurrentFrameResource().destroyeSemaphores.push_back(semaphore);
}

void DeviceVulkan::ReleaseShader(Shader* shader)
{
    LOCK();
    CurrentFrameResource().destroyedShaders.push_back(shader->GetModule());
}

void DeviceVulkan::RecycleSemaphore(VkSemaphore semaphore)
{
    LOCK();
    CurrentFrameResource().recycledSemaphroes.push_back(semaphore);
}

void DeviceVulkan::ReleaseEvent(VkEvent ent)
{
    LOCK();
    CurrentFrameResource().recyledEvents.push_back(ent);
}

void DeviceVulkan::ReleaseFrameBufferNolock(VkFramebuffer buffer)
{
    CurrentFrameResource().destroyedFrameBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseImageNolock(VkImage image)
{
    CurrentFrameResource().destroyedImages.push_back(image);
}

void DeviceVulkan::ReleaseImageViewNolock(VkImageView imageView)
{
    CurrentFrameResource().destroyedImageViews.push_back(imageView);
}

void DeviceVulkan::ReleaseFenceNolock(VkFence fence, bool isWait)
{
    if (isWait)
    {
        vkResetFences(device, 1, &fence);
        fencePoolManager.Recyle(fence);
    }
    else
    {
        CurrentFrameResource().recyleFences.push_back(fence);
    }
}

void DeviceVulkan::ReleaseBufferNolock(VkBuffer buffer)
{
    CurrentFrameResource().destroyedBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseBufferViewNolock(VkBufferView bufferView)
{
    CurrentFrameResource().destroyedBufferViews.push_back(bufferView);
}

void DeviceVulkan::ReleaseSamplerNolock(VkSampler sampler)
{
    CurrentFrameResource().destroyedSamplers.push_back(sampler);
}

void DeviceVulkan::ReleaseDescriptorPoolNolock(VkDescriptorPool pool)
{
    CurrentFrameResource().destroyedDescriptorPool.push_back(pool);
}

void DeviceVulkan::ReleasePipelineNolock(VkPipeline pipeline)
{
    CurrentFrameResource().destroyedPipelines.push_back(pipeline);
}

void DeviceVulkan::FreeMemoryNolock(const DeviceAllocation& allocation)
{
    CurrentFrameResource().destroyedAllocations.push_back(allocation);
}

void DeviceVulkan::ReleaseBindlessResourceNoLock(I32 index, BindlessReosurceType type)
{
    CurrentFrameResource().destroyedBindlessResources.push_back({ index, type });
}

void DeviceVulkan::ReleaseSemaphoreNolock(VkSemaphore semaphore)
{
    CurrentFrameResource().destroyeSemaphores.push_back(semaphore);
}

void DeviceVulkan::RecycleSemaphoreNolock(VkSemaphore semaphore)
{
    CurrentFrameResource().recycledSemaphroes.push_back(semaphore);
}

void DeviceVulkan::ReleaseEventNolock(VkEvent ent)
{
    CurrentFrameResource().recyledEvents.push_back(ent);
}


void* DeviceVulkan::MapBuffer(const Buffer& buffer, MemoryAccessFlags flags)
{
    return memory.MapMemory(buffer.allocation, flags, 0, buffer.GetCreateInfo().size);
}

void DeviceVulkan::UnmapBuffer(const Buffer& buffer, MemoryAccessFlags flags)
{
    memory.UnmapMemory(buffer.allocation, flags, 0, buffer.GetCreateInfo().size);
}

bool DeviceVulkan::IsImageFormatSupported(VkFormat format, VkFormatFeatureFlags required, VkImageTiling tiling)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    auto flags = tiling == VK_IMAGE_TILING_OPTIMAL ? props.optimalTilingFeatures : props.linearTilingFeatures;
    return (flags & required) == required;
}

U64 DeviceVulkan::GetMinOffsetAlignment() const
{
    return (U64)std::max((VkDeviceSize)1, features.properties2.properties.limits.minStorageBufferOffsetAlignment);
}

ImmutableSampler* DeviceVulkan::GetStockSampler(StockSampler type)
{
    return stockSamplers[(int)type];
}

uint64_t DeviceVulkan::GenerateCookie()
{
#ifdef VULKAN_MT
    return STATIC_COOKIE.fetch_add(16, std::memory_order_relaxed) + 16;
#else
    STATIC_COOKIE += 16;
    return STATIC_COOKIE;
#endif
}

ShaderManager& DeviceVulkan::GetShaderManager()
{
    return shaderManager;
}

SemaphorePtr DeviceVulkan::GetAndConsumeReleaseSemaphore()
{
    auto ret = std::move(wsi.release);
    wsi.release.reset();
    return ret;
}

VkQueue DeviceVulkan::GetPresentQueue() const
{
    return wsi.presentQueue;
}

void DeviceVulkan::WaitIdle()
{
    DRAIN_FRAME_LOCK();
    WaitIdleNolock();
}

void DeviceVulkan::WaitIdleNolock()
{
    if (!frameResources.empty())
        EndFrameContextNolock();

    if (device != VK_NULL_HANDLE)
    {
        auto ret = vkDeviceWaitIdle(device);
        if (ret != VK_SUCCESS)
            Logger::Error("vkDeviceWaitIdle failed:%d", ret);
        if (ret == VK_ERROR_DEVICE_LOST)
            LogDeviceLost();
    }

    // Clear wait semaphores
    for (auto& queueData : queueDatas)
    {
        for (auto& sem : queueData.waitSemaphores)
            vkDestroySemaphore(device, sem->GetSemaphore(), nullptr);

        queueData.waitSemaphores.clear();
        queueData.waitStages.clear();
    }

    // Clear buffer pools
    vboPool.Reset();
    iboPool.Reset();
    uboPool.Reset();
    stagingPool.Reset();
    storagePool.Reset();
    for (auto& frame : frameResources)
    {
        frame->vboBlocks.clear();
        frame->iboBlocks.clear();
        frame->uboBlocks.clear();
    
        frame->storageBlocks.clear();
        frame->storageBlockMap.clear();
    }

    frameBufferAllocator.Clear();
    transientAllocator.Clear();

#ifdef VULKAN_MT
    // descriptor set begin
    for (auto& set : descriptorSetAllocators.GetReadOnly())
        set.Clear();
    for (auto& set : descriptorSetAllocators.GetReadWrite())
        set.Clear();
#else
    for (auto& allocator : descriptorSetAllocators)
        allocator.second->Clear();
#endif

    for (auto& frame : frameResources)
    {
        frame->waitFences.clear();
        frame->Begin();
    }
}

bool DeviceVulkan::IsSwapchainTouched()
{
    return wsi.consumed;
}

void DeviceVulkan::MoveReadWriteCachesToReadOnly()
{
#ifdef VULKAN_MT
    descriptorSetAllocators.MoveToReadOnly();
    pipelineLayouts.MoveToReadOnly();
    shaders.MoveToReadOnly();
    programs.MoveToReadOnly();
    for (auto& program : programs.GetReadOnly())
        program.MoveToReadOnly();   // ShaderProgram::Pipelines

    renderPasses.MoveToReadOnly();
    shaderManager.MoveToReadOnly();
    immutableSamplers.MoveToReadOnly();
#endif
}

void DeviceVulkan::InitTimelineSemaphores()
{
    VkSemaphoreTypeCreateInfo timelineCreateInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    createInfo.pNext = &timelineCreateInfo;
    createInfo.flags = 0;

    for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
    {
        if (vkCreateSemaphore(device, &createInfo, nullptr, &queueDatas[i].timelineSemaphore ) != VK_SUCCESS)
            Logger::Error("Failed to create timeline semaphore");
    }
}

void DeviceVulkan::DeinitTimelineSemaphores()
{
    for (auto& queueData : queueDatas)
    {
        if (queueData.timelineSemaphore != VK_NULL_HANDLE)
            vkDestroySemaphore(device, queueData.timelineSemaphore, nullptr);

        queueData.timelineSemaphore = VK_NULL_HANDLE;
    }

    for (auto& frame : frameResources)
    {
        for (auto& value : frame->timelineValues)
            value = 0;
        for (auto& timeline : frame->timelineSemaphores)
            timeline = VK_NULL_HANDLE;
    }
}

void DeviceVulkan::SubmitNolock(CommandListPtr cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore)
{
    QueueIndices queueIndex = GetPhysicalQueueType(cmd->GetQueueType());
    auto& submissions = CurrentFrameResource().submissions[queueIndex];

    cmd->EndCommandBuffer();

    submissions.push_back(std::move(cmd));

    InternalFence signalledFence;
    if (fence != nullptr || semaphoreCount > 0) 
    {
        SubmitQueue(queueIndex, 
            fence != nullptr ? &signalledFence : nullptr, 
            semaphoreCount, semaphore);
    }

    if (fence)
    {
        ASSERT(!(*fence)); // Fence must be empty
        if (signalledFence.value > 0)
            *fence = FencePtr(fencePool.allocate(*this, signalledFence.value, signalledFence.timeline));
    }

    DecrementFrameCounter();
}

void DeviceVulkan::SubmitQueue(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores)
{
    // Flush/Submit transfer queue
    if (queueIndex != QueueIndices::QUEUE_INDEX_TRANSFER)
        FlushFrame(QueueIndices::QUEUE_INDEX_TRANSFER);

    auto& submissions = CurrentFrameResource().submissions[queueIndex];
    if (submissions.empty())
    {
        if (fence != nullptr || semaphoreCount > 0)
            SubmitEmpty(queueIndex, fence, semaphoreCount, semaphores);
        return;
    }

    QueueData& queueData = queueDatas[queueIndex];

    // Update timeline value
    VkSemaphore timelineSemaphore = queueData.timelineSemaphore;
    U64 timeline = ++queueData.timeline;
    CurrentFrameResource().timelineValues[queueIndex] = queueData.timeline;

    BatchComposer batchComposer;

    // Collect wait semaphores
    WaitSemaphores waitSemaphores = {};
    CollectWaitSemaphores(queueData, waitSemaphores);
    batchComposer.AddWaitSemaphores(waitSemaphores);

    // Compose submit batches
    VkQueue queue = queueInfo.queues[queueIndex];
    for (int i = 0; i < submissions.size(); i++)
    {
        auto& cmd = submissions[i];
        VkPipelineStageFlags wsiStages = cmd->GetSwapchainStages();  

        // For first command buffer which uses WSI, need to emit WSI acquire wait 
        if (wsiStages != 0 && !wsi.consumed)
        {
            // Acquire semaphore
            if (wsi.acquire && wsi.acquire->GetSemaphore() != VK_NULL_HANDLE)
            {
                ASSERT(wsi.acquire->IsSignalled());
                batchComposer.AddWaitSemaphore(wsi.acquire, wsiStages);
                if (wsi.acquire->GetSemaphoreType() == VK_SEMAPHORE_TYPE_BINARY_KHR)
                {
                    if (wsi.acquire->CanRecycle())
                        CurrentFrameResource().recycledSemaphroes.push_back(wsi.acquire->GetSemaphore());
                    else
                        CurrentFrameResource().destroyeSemaphores.push_back(wsi.acquire->GetSemaphore());
                }
                wsi.acquire->Consume();
                wsi.acquire.reset();
            }

            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());

            // Release semaphore
            VkSemaphore release = semaphoreManager.Requset();
            wsi.release = SemaphorePtr(semaphorePool.allocate(*this, release, true));
            wsi.release->SetInternalSyncObject();
            batchComposer.AddSignalSemaphore(release, 0);

            wsi.presentQueue = queue;
            wsi.consumed = true;
        }
        else
        {
            ASSERT(wsiStages == 0);
            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());
        }
    }

    EmitQueueSignals(batchComposer, timelineSemaphore, timeline, fence, semaphoreCount, semaphores);

    VkResult ret = SubmitBatches(batchComposer, queue, VK_NULL_HANDLE);
    if (ret != VK_SUCCESS)
        Logger::Error("Submit queue failed (code: %d)", (int)ret);
    if (ret == VK_ERROR_DEVICE_LOST)
        LogDeviceLost();

    submissions.clear();
}

void DeviceVulkan::SubmitEmpty(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores)
{
    QueueData& queueData = queueDatas[queueIndex];

    // Upate timeline value
    VkSemaphore timelineSemaphore = queueData.timelineSemaphore;
    U64 timeline = ++queueData.timeline;
    CurrentFrameResource().timelineValues[queueIndex] = queueData.timeline;

    // Colloect wait semaphores
    WaitSemaphores waitSemaphores = {};
    CollectWaitSemaphores(queueData, waitSemaphores);
    BatchComposer batchComposer;
    batchComposer.AddWaitSemaphores(waitSemaphores);

    // Emit queue signals
    EmitQueueSignals(batchComposer, timelineSemaphore, timeline, fence, semaphoreCount, semaphores);

    // Submit batches
    VkQueue queue = queueInfo.queues[queueIndex];
    VkResult ret = SubmitBatches(batchComposer, queue, VK_NULL_HANDLE);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Submit empty queue failed: ");
        if (ret == VK_ERROR_DEVICE_LOST)
            LogDeviceLost();
    }
}

VkResult DeviceVulkan::SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence)
{
    auto& submits = composer.Bake();
    return vkQueueSubmit(queue, (U32)submits.size(), submits.data(), fence);
}

void DeviceVulkan::SubmitStaging(CommandListPtr& cmd, VkBufferUsageFlags usage, bool flush)
{
    // Check source buffer's usage to decide which queues (Graphics/Compute) need to wait
    VkAccessFlags access = Buffer::BufferUsageToPossibleAccess(usage);
    VkPipelineStageFlags stages = Buffer::BufferUsageToPossibleStages(usage);
    VkQueue srcQueue = queueInfo.queues[GetPhysicalQueueType(cmd->GetQueueType())];

    if (srcQueue == queueInfo.queues[QueueIndices::QUEUE_INDEX_GRAPHICS] &&
        srcQueue == queueInfo.queues[QueueIndices::QUEUE_INDEX_COMPUTE])
    {
        cmd->Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, stages, access);
        SubmitNolock(cmd, nullptr, 0, nullptr);
        return;
    }

    auto computeStages = stages &
            (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
             VK_PIPELINE_STAGE_TRANSFER_BIT |
             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

    auto computeAccess = access &
            (VK_ACCESS_SHADER_READ_BIT |
             VK_ACCESS_SHADER_WRITE_BIT |
             VK_ACCESS_TRANSFER_READ_BIT |
             VK_ACCESS_UNIFORM_READ_BIT |
             VK_ACCESS_TRANSFER_WRITE_BIT |
             VK_ACCESS_INDIRECT_COMMAND_READ_BIT);

    auto graphicsStages = stages;

    if (srcQueue == queueInfo.queues[QueueIndices::QUEUE_INDEX_GRAPHICS])
    {
        // While SyncBuffer in graphics queue 
        cmd->Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, stages, access);
        if (computeStages != 0)
        {
            SemaphorePtr sem;
            SubmitNolock(cmd, nullptr, 1, &sem);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_COMPUTE, sem, stages, flush);
        }
        else
        {
            SubmitNolock(cmd, nullptr, 0, nullptr);
        }
    }
    else if (srcQueue == queueInfo.queues[QueueIndices::QUEUE_INDEX_COMPUTE])
    {
        // While SyncBuffer in Compute queue 
        cmd->Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, computeStages, computeAccess);
        if (stages != 0)
        {
            SemaphorePtr sem;
            SubmitNolock(cmd, nullptr, 1, &sem);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_GRAPHICS, sem, stages, flush);
        }
        else
        {
            SubmitNolock(cmd, nullptr, 0, nullptr);
        }
    }
    else
    {
        if (graphicsStages != 0 && computeStages != 0)
        {
            SemaphorePtr semaphores[2];
            SubmitNolock(cmd, nullptr, 2, semaphores);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_GRAPHICS, semaphores[0], graphicsStages, flush);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_COMPUTE, semaphores[1], computeStages, flush);
        }
        else if (graphicsStages != 0)
        {
            SemaphorePtr semaphore;
            SubmitNolock(cmd, nullptr, 1, &semaphore);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_GRAPHICS, semaphore, graphicsStages, flush);
        }
        else if (computeStages != 0)
        {
            SemaphorePtr semaphore;
            SubmitNolock(cmd, nullptr, 1, &semaphore);
            AddWaitSemaphoreNolock(QueueIndices::QUEUE_INDEX_COMPUTE, semaphore, computeStages, flush);
        }
        else
        {
            SubmitNolock(cmd, nullptr, 0, nullptr);
        }
    }
}

static const char* queueNameTable[] = {
    "Graphics",
    "Compute",
    "Transfer",
};

void DeviceVulkan::LogDeviceLost()
{
    if (!features.supportDevieDiagnosticCheckpoints)
        return;

    for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
    {
        if (queueInfo.queues[i] == VK_NULL_HANDLE)
            continue;

        uint32_t count;
        vkGetQueueCheckpointDataNV(queueInfo.queues[i], &count, nullptr);
        std::vector<VkCheckpointDataNV> checkpointDatas(count);
        for (auto& data : checkpointDatas)
            data.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
        vkGetQueueCheckpointDataNV(queueInfo.queues[i], &count, checkpointDatas.data());

        if (!checkpointDatas.empty())
        {
            Logger::Info("Checkpoints for %s queue:\n", queueNameTable[i]);
            for (auto& d : checkpointDatas)
                Logger::Info("Stage %u:\n%s\n", d.stage, static_cast<const char*>(d.pCheckpointMarker));
        }
    }
}

void DeviceVulkan::CollectWaitSemaphores(QueueData& data, WaitSemaphores& waitSemaphores)
{
    for (int i = 0; i < data.waitSemaphores.size(); i++)
    {
        SemaphorePtr& semaphore = data.waitSemaphores[i];
        VkSemaphore vkSemaphore = semaphore->Consume(); // Semaphore is signalled
        if (semaphore->GetSemaphoreType() == VK_SEMAPHORE_TYPE_BINARY_KHR)
        {
            if (semaphore->CanRecycle())
                RecycleSemaphoreNolock(vkSemaphore);
            else
                ReleaseSemaphoreNolock(vkSemaphore);

            waitSemaphores.binaryWaits.push_back(vkSemaphore);
            waitSemaphores.binaryWaitStages.push_back(data.waitStages[i]);
        }
        else
        {
            waitSemaphores.timelineWaits.push_back(vkSemaphore);
            waitSemaphores.timelineWaitStages.push_back(data.waitStages[i]);
            waitSemaphores.timelineWaitCounts.push_back(semaphore->GetTimeLine());
        }
    }
    data.waitSemaphores.clear();
    data.waitStages.clear();
}

void DeviceVulkan::EmitQueueSignals(BatchComposer& composer, VkSemaphore sem, U64 timeline, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores)
{
    composer.AddSignalSemaphore(sem, timeline);

    if (fence != nullptr)
    {
        fence->timeline = sem;
        fence->value = timeline;
    }

    // Add cleared semaphores, they will signal when submission finished
    for (unsigned i = 0; i < semaphoreCount; i++)
    {
        ASSERT(!semaphores[i]);
        semaphores[i] = SemaphorePtr(semaphorePool.allocate(*this, timeline, sem));
        semaphores[i]->Signal();
    }
}

DeviceVulkan::FrameResource::FrameResource(DeviceVulkan& device_, U32 frameIndex_) : 
    device(device_),
    frameIndex(frameIndex_)
{
    for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
    {
        timelineSemaphores[queueIndex] = device_.queueDatas[queueIndex].timelineSemaphore;
    }
}

DeviceVulkan::FrameResource::~FrameResource()
{
    Begin();

    for (auto pools : cmdPools)
        pools.DeleteAll();
}

void DeviceVulkan::FrameResource::Begin()
{
    VkDevice vkDevice = device.device;

    // Wait for timeline semaphores
    bool hasTimeline = true;
    for (auto& sem : timelineSemaphores)
    {
        if (sem == VK_NULL_HANDLE)
        {
            hasTimeline = false;
            break;
        }
    }

    if (hasTimeline)
    {
        VkSemaphoreWaitInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
        VkSemaphore sems[QUEUE_INDEX_COUNT];
        U64 values[QUEUE_INDEX_COUNT];
        for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
        {
            if (timelineValues[i])
            {
                sems[info.semaphoreCount] = timelineSemaphores[i];
                values[info.semaphoreCount] = timelineValues[i];
                info.semaphoreCount++;
            }
        }

        if (info.semaphoreCount > 0)
        {
            info.pSemaphores = sems;
            info.pValues = values;
            vkWaitSemaphores(vkDevice, &info, UINT64_MAX);
        }
    }

    // Wait for submiting
    if (!waitFences.empty())
    {
        vkWaitForFences(device.device, (U32)waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
        waitFences.clear();
    }

    // Reset recyle fences
    if (!recyleFences.empty())
    {
        vkResetFences(device.device, (U32)recyleFences.size(), recyleFences.data());
        for (auto& fence : recyleFences)
            device.fencePoolManager.Recyle(fence);
        recyleFences.clear();
    }

    // Reset command pools
    for (auto& cmdPool : cmdPools)
    {
        Array<CommandPool*> pools;
        cmdPool.GetNotNullValues(pools);
        for (auto pool : pools)
            pool->BeginFrame();
    }

    // Recycle buffer blocks
    for (auto& block : vboBlocks)
        device.vboPool.RecycleBlock(block);
    for (auto& block : iboBlocks)
        device.iboPool.RecycleBlock(block);
    for (auto& block : uboBlocks)
        device.uboPool.RecycleBlock(block);
    for (auto& block : stagingBlocks)
        device.stagingPool.RecycleBlock(block);
    for (auto& block : storageBlocks)
        device.storagePool.RecycleBlock(block);

    vboBlocks.clear();
    iboBlocks.clear();
    uboBlocks.clear();
    stagingBlocks.clear();
    storageBlocks.clear();

    // Clear destroyed resources
    for (auto& buffer : destroyedFrameBuffers)
        vkDestroyFramebuffer(vkDevice, buffer, nullptr);
    for (auto& sampler : destroyedSamplers)
        vkDestroySampler(vkDevice, sampler, nullptr);
    for (auto& image : destroyedImages)
        vkDestroyImage(vkDevice, image, nullptr);
    for (auto& imageView : destroyedImageViews)
        vkDestroyImageView(vkDevice, imageView, nullptr);
    for (auto& buffer : destroyedBuffers)
        vkDestroyBuffer(vkDevice, buffer, nullptr);
    for (auto& bufferView : destroyedBufferViews)
        vkDestroyBufferView(vkDevice, bufferView, nullptr);
    for (auto& pool : destroyedDescriptorPool)
        vkDestroyDescriptorPool(vkDevice, pool, nullptr);
    for (auto& semaphore : destroyeSemaphores)
        vkDestroySemaphore(vkDevice, semaphore, nullptr);
    for (auto& pipeline : destroyedPipelines)
        vkDestroyPipeline(vkDevice, pipeline, nullptr);
    for (auto& ent : recyledEvents)
        device.eventManager.Recyle(ent);
    for (auto& allocation : destroyedAllocations)
        allocation.Free(device.memory);
    for (auto& shader : destroyedShaders)
        vkDestroyShaderModule(vkDevice, shader, nullptr);

    for (auto& kvp : destroyedBindlessResources)
    {
        auto heap = device.GetBindlessDescriptorHeap(kvp.second);
        if (heap)
            heap->Free(kvp.first);
    }

    destroyedFrameBuffers.clear();
    destroyedSamplers.clear();
    destroyedImages.clear();
    destroyedImageViews.clear();
    destroyedBuffers.clear();
    destroyedBufferViews.clear();
    destroyedDescriptorPool.clear();
    destroyeSemaphores.clear();
    destroyedPipelines.clear();
    recyledEvents.clear();
    destroyedAllocations.clear();
    destroyedBindlessResources.clear();
    destroyedShaders.clear();

    // reset persistent storage blocks
    for (auto& kvp : storageBlockMap)
        kvp.second.offset = 0;

    // reset recyle semaphores
    auto& recyleSemaphores = recycledSemaphroes;
    for (auto& semaphore : recyleSemaphores)
        device.semaphoreManager.Recyle(semaphore);
    recyleSemaphores.clear();
}

BatchComposer::BatchComposer()
{
    // Add a default empty submission
    submits.emplace_back();
}

void BatchComposer::BeginBatch()
{
    // create a new VkSubmitInfo
    auto& submitInfo = submitInfos[submitIndex];
    if (!submitInfo.commandLists.empty() || !submitInfo.waitSemaphores.empty())
    {
        submitIndex = (U32)submits.size();
        submits.emplace_back();
    }
}

static bool hasTimelineSemaphore(const std::vector<uint64_t>& counts)
{
    return std::find_if(counts.begin(), counts.end(), [](uint64_t count) {
        return count != 0;
    }) != counts.end();
}

bool BatchComposer::hasTimelineSemaphoreInBatch(U32 index) const
{
    return hasTimelineSemaphore(submitInfos[index].waitCounts) ||
        hasTimelineSemaphore(submitInfos[index].signalCounts);
}

void BatchComposer::AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages)
{
    if (!submitInfos[submitIndex].commandLists.empty() || 
        !submitInfos[submitIndex].signalSemaphores.empty())
        BeginBatch();

    bool isTimeline = sem->GetSemaphoreType() == VK_SEMAPHORE_TYPE_TIMELINE_KHR;
    submitInfos[submitIndex].waitSemaphores.push_back(sem->GetSemaphore());
    submitInfos[submitIndex].waitStages.push_back(stages);
    submitInfos[submitIndex].waitCounts.push_back(isTimeline ? sem->GetTimeLine() : 0);
}

void BatchComposer::AddWaitSemaphores(WaitSemaphores& semaphores)
{
    if (!semaphores.binaryWaits.empty())
    {
        for (int i = 0; i < semaphores.binaryWaits.size(); i++)
        {
            submitInfos[submitIndex].waitSemaphores.push_back(semaphores.binaryWaits[i]);
            submitInfos[submitIndex].waitStages.push_back(semaphores.binaryWaitStages[i]);
            submitInfos[submitIndex].waitCounts.push_back(0);
        }
    }

    if (!semaphores.timelineWaits.empty())
    {
        for (int i = 0; i < semaphores.timelineWaits.size(); i++)
        {
            submitInfos[submitIndex].waitSemaphores.push_back(semaphores.timelineWaits[i]);
            submitInfos[submitIndex].waitStages.push_back(semaphores.timelineWaitStages[i]);
            submitInfos[submitIndex].waitCounts.push_back(semaphores.timelineWaitCounts[i]);
        }
    }
}

void BatchComposer::AddSignalSemaphore(VkSemaphore sem, U64 timeline)
{
    submitInfos[submitIndex].signalSemaphores.push_back(sem);
    submitInfos[submitIndex].signalCounts.push_back(timeline);
}

void BatchComposer::AddCommandBuffer(VkCommandBuffer buffer)
{
    // 如果存在Signal semaphores，先将之前的cmds batch
    if (!submitInfos[submitIndex].signalSemaphores.empty())
        BeginBatch();

    submitInfos[submitIndex].commandLists.push_back(buffer);
}

std::vector<VkSubmitInfo>& BatchComposer::Bake()
{
    for (size_t index = 0; index < submits.size(); index++)
    {
        auto& submitInfo = submitInfos[index];
        auto& timelineInfo = timelineInfos[index];

        auto& submit = submits[index];
        submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };

        if (hasTimelineSemaphoreInBatch(index))
        {
            timelineInfo = {};
            timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
            timelineInfo.pNext = nullptr;
            timelineInfo.waitSemaphoreValueCount = (U32)submitInfo.waitCounts.size();
            timelineInfo.pWaitSemaphoreValues = submitInfo.waitCounts.data();
            timelineInfo.signalSemaphoreValueCount = (U32)submitInfo.signalCounts.size();
            timelineInfo.pSignalSemaphoreValues = submitInfo.signalCounts.data();

            submit.pNext = &timelineInfo;
        }

        submit.commandBufferCount = (U32)submitInfo.commandLists.size();
        submit.pCommandBuffers = submitInfo.commandLists.data();
        submit.waitSemaphoreCount = (U32)submitInfo.waitSemaphores.size();
        submit.pWaitSemaphores = submitInfo.waitSemaphores.data();
        submit.pWaitDstStageMask = submitInfo.waitStages.data();
        submit.signalSemaphoreCount = (U32)submitInfo.signalSemaphores.size();
        submit.pSignalSemaphores = submitInfo.signalSemaphores.data();
    }

    // Remove empty submissions
    size_t submitCount = 0;
    for (size_t i = 0, n = submits.size(); i < n; i++)
    {
        if (submits[i].waitSemaphoreCount || 
            submits[i].signalSemaphoreCount || 
            submits[i].commandBufferCount)
        {
            if (i != submitCount)
                submits[submitCount] = submits[i];

            submitCount++;
        }
    }
    if (submitCount != submits.size())
        submits.resize(submitCount);
    return submits;
}

void DeviceVulkan::AddFrameCounter()
{
    frameCounter++;
}

void DeviceVulkan::DecrementFrameCounter()
{
    ASSERT(frameCounter > 0);
    frameCounter--;
#ifdef VULKAN_MT
    cond.notify_all();
#endif
}

void DeviceVulkan::InitStockSamplers()
{
    for (int i = 0; i < (int)StockSampler::Count; i++)
        InitStockSampler(StockSampler(i));
}

void DeviceVulkan::InitStockSampler(StockSampler type)
{
    SamplerCreateInfo info = {};
    info.maxLod = VK_LOD_CLAMP_NONE;
    info.maxAnisotropy = 1.0f;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.compareEnable = false;

    switch (type)
    {
    case GPU::StockSampler::NearestClamp:
    case GPU::StockSampler::NearestWrap:
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        break;
    case GPU::StockSampler::PointClamp:
    case GPU::StockSampler::PointWrap:
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        break;
    }

    switch (type)
    {
    case GPU::StockSampler::NearestClamp:
    case GPU::StockSampler::PointClamp:
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        break;
    case GPU::StockSampler::NearestWrap:
    case GPU::StockSampler::PointWrap:
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }

    stockSamplers[(int)type] = RequestImmutableSampler(info);
}

void DeviceVulkan::InitBindless()
{
    DescriptorSetLayout layout;
    layout.isBindless = true;

    U32 stagesForSets[VULKAN_NUM_BINDINGS] = { VK_SHADER_STAGE_ALL };
    if (features.features_1_2.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE] = 1;
        bindlessSampledImagesSetAllocator = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
        bindlessHandler.bindlessSampledImages.Init(*this, bindlessSampledImagesSetAllocator);
    }
    if (features.features_1_2.descriptorBindingStorageBufferUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[DESCRIPTOR_SET_TYPE_STORAGE_BUFFER] = 1;
        bindlessStorageBuffersSetAllocator = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
        bindlessHandler.bindlessStorageBuffers.Init(*this, bindlessStorageBuffersSetAllocator);
    }
    if (features.features_1_2.descriptorBindingStorageImageUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[DESCRIPTOR_SET_TYPE_STORAGE_IMAGE] = 1;
        bindlessStorageImagesSetAllocator = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
    if (features.features_1_2.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[DESCRIPTOR_SET_TYPE_SAMPLER] = 1;
        bindlessSamplersSetAllocator = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
}

BindlessDescriptorHeap* DeviceVulkan::GetBindlessDescriptorHeap(BindlessReosurceType type)
{
    switch (type)
    {
    case BindlessReosurceType::SampledImage:
        return &bindlessHandler.bindlessSampledImages;
    case BindlessReosurceType::StorageBuffer:
        return &bindlessHandler.bindlessStorageBuffers;
    default:
        break;
    }
    return nullptr;
}

const char* GetPipelineCachePath()
{
    static const std::string PIPELINE_CACHE_PATH = ".export/pipeline_cache.bin";
    return PIPELINE_CACHE_PATH.c_str();
}

void DeviceVulkan::InitPipelineCache()
{
    auto fs = systemHandles.fileSystem;
    const char* cachePath = GetPipelineCachePath();
    if (!fs->FileExists(cachePath))
    {
        if (!InitPipelineCache(nullptr, 0))
            Logger::Warning("Failed to init pipeline cache.");
        return;
    }

    OutputMemoryStream mem;
    if (!fs->LoadContext(cachePath, mem))
    {
        Logger::Warning("Failed to init pipeline cache.");
        return;
    }

    if (!InitPipelineCache(mem.Data(), mem.Size()))
        Logger::Warning("Failed to init pipeline cache.");
}

bool DeviceVulkan::InitPipelineCache(const U8* data, size_t size)
{
    if (data != nullptr && size > 0)
    {
        uint32_t headerLength = 0;
        uint32_t cacheHeaderVersion = 0;
        uint32_t vendorID = 0;
        uint32_t deviceID = 0;
        uint8_t pipelineCacheUUID[VK_UUID_SIZE] = {};

        std::memcpy(&headerLength, (uint8_t*)data + 0, 4);
        std::memcpy(&cacheHeaderVersion, (uint8_t*)data + 4, 4);
        std::memcpy(&vendorID, (uint8_t*)data + 8, 4);
        std::memcpy(&deviceID, (uint8_t*)data + 12, 4);
        std::memcpy(pipelineCacheUUID, (uint8_t*)data + 16, VK_UUID_SIZE);

        bool badCache = false;
        if (headerLength <= 0)
        {
            badCache = true;
        }

        if (cacheHeaderVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
        {
            badCache = true;
        }

        if (vendorID != features.properties2.properties.vendorID)
        {
            badCache = true;
        }

        if (deviceID != features.properties2.properties.deviceID)
        {
            badCache = true;
        }

        if (memcmp(pipelineCacheUUID, features.properties2.properties.pipelineCacheUUID, sizeof(pipelineCacheUUID)) != 0)
        {
            badCache = true;
        }

        if (badCache)
        {
            size = 0;
            data = nullptr;
        }
    }

    VkPipelineCacheCreateInfo createInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    createInfo.initialDataSize = size;
    createInfo.pInitialData = data;

    if (pipelineCache != VK_NULL_HANDLE)
        vkDestroyPipelineCache(device, pipelineCache, nullptr);
    pipelineCache = VK_NULL_HANDLE;
    return vkCreatePipelineCache(device, &createInfo, nullptr, &pipelineCache) == VK_SUCCESS;
}

void DeviceVulkan::FlushPipelineCache()
{
    auto fs = systemHandles.fileSystem;

    // Get size of pipeline cache
    size_t size{};
    VkResult res = vkGetPipelineCacheData(device, pipelineCache, &size, nullptr);
    assert(res == VK_SUCCESS);

    // Get data of pipeline cache 
    std::vector<uint8_t> data(size);
    res = vkGetPipelineCacheData(device, pipelineCache, &size, data.data());
    assert(res == VK_SUCCESS);

    // Write pipeline cache data to a file in binary format
    const char* cachePath = GetPipelineCachePath();
    auto file = fs->OpenFile(cachePath, FileFlags::DEFAULT_WRITE);
    if (!file->IsValid())
    {
        Logger::Error("Failed to save pipeline cache.");
        return;
    }
    file->Write(data.data(), size);
    file->Close();
}

void DeviceVulkan::SyncPendingBufferBlocks()
{
    if (pendingBufferBlocks.vbo.empty() ||
        pendingBufferBlocks.ubo.empty() ||
        pendingBufferBlocks.ibo.empty())
        return;

    VkBufferUsageFlags usage = 0;
    auto cmd = RequestCommandListNolock(QueueType::QUEUE_TYPE_ASYNC_TRANSFER);
    cmd->BeginEvent("Buffer_block_sync");
    for (auto& block : pendingBufferBlocks.vbo)
    {
        ASSERT(block.offset != 0);
        cmd->CopyBuffer(*block.gpu, *block.cpu);
        usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    pendingBufferBlocks.vbo.clear();

    for (auto& block : pendingBufferBlocks.ibo)
    {
        ASSERT(block.offset != 0);
        cmd->CopyBuffer(*block.gpu, *block.cpu);
        usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    pendingBufferBlocks.ibo.clear();

    for (auto& block : pendingBufferBlocks.ubo)
    {
        ASSERT(block.offset != 0);
        cmd->CopyBuffer(*block.gpu, *block.cpu);
        usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    pendingBufferBlocks.ubo.clear();

    cmd->EndEvent();

    if (usage != 0)
        SubmitStaging(cmd, usage, false);
}

void TransientAttachmentAllcoator::BeginFrame()
{
    attachments.BeginFrame();
}

void TransientAttachmentAllcoator::Clear()
{
    attachments.Clear();
}

ImagePtr TransientAttachmentAllcoator::RequsetAttachment(U32 w, U32 h, VkFormat format, U32 index, U32 samples, U32 layers)
{
    HashCombiner hash;
    hash.HashCombine(w);
    hash.HashCombine(h);
    hash.HashCombine(format);
    hash.HashCombine(index);
    hash.HashCombine(samples);
    hash.HashCombine(layers);

    LOCK();
    auto* node = attachments.Requset(hash.Get());
    if (node != nullptr)
        return node->image;

    auto imageInfo = ImageCreateInfo::TransientRenderTarget(w, h, format);
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(samples);
    imageInfo.layers = layers;

    node = attachments.Emplace(hash.Get(), device.CreateImage(imageInfo, nullptr));
    node->image->SetInternalSyncObject();
    node->image->GetImageView().SetInternalSyncObject();
    return node->image;
}

void FrameBufferAllocator::BeginFrame()
{
    framebuffers.BeginFrame();
}

void FrameBufferAllocator::Clear()
{
    framebuffers.Clear();
}

FrameBuffer& FrameBufferAllocator::RequestFrameBuffer(const RenderPassInfo& info)
{
    HashCombiner hash;
    RenderPass& renderPass = device.RequestRenderPass(info, true);
    hash.HashCombine(renderPass.GetHash());

    // Get color attachments hash
    for (U32 i = 0; i < info.numColorAttachments; i++)
        hash.HashCombine(info.colorAttachments[i]->GetCookie());

    // Get depth stencil hash
    if (info.depthStencil)
        hash.HashCombine(info.depthStencil->GetCookie());

    LOCK();
    FrameBufferNode* node = framebuffers.Requset(hash.Get());
    if (node != nullptr)
        return *node;

    return *framebuffers.Emplace(hash.Get(), device, renderPass, info);
}

}
}