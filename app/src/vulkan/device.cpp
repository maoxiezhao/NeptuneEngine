#include "device.h"
#include "utils\hash.h"

namespace 
{
	static const QueueIndices QUEUE_FLUSH_ORDER[] = {
		QUEUE_INDEX_GRAPHICS,
		QUEUE_INDEX_COMPUTE
	};

    QueueIndices ConvertQueueTypeToIndices(QueueType type)
    {
        return static_cast<QueueIndices>(type);
    }

    std::vector<const char*> GetOptimalValidationLayers(const std::vector<VkLayerProperties>& layerProperties)
    {
        std::vector<std::vector<const char*>> validationLayerPriorityList =
        {
            // The preferred validation layer is "VK_LAYER_KHRONOS_validation"
            {"VK_LAYER_KHRONOS_validation"},

            // Otherwise we fallback to using the LunarG meta layer
            {"VK_LAYER_LUNARG_standard_validation"},

            // Otherwise we attempt to enable the individual layers that compose the LunarG meta layer since it doesn't exist
            {
                "VK_LAYER_GOOGLE_threading",
                "VK_LAYER_LUNARG_parameter_validation",
                "VK_LAYER_LUNARG_object_tracker",
                "VK_LAYER_LUNARG_core_validation",
                "VK_LAYER_GOOGLE_unique_objects",
            },

            // Otherwise as a last resort we fallback to attempting to enable the LunarG core layer
            {"VK_LAYER_LUNARG_core_validation"}
        };

        auto ValidateLayers = [](const std::vector<const char*>& required,
            const std::vector<VkLayerProperties>& available)
        {
            for (auto layer : required)
            {
                bool found = false;
                for (auto& available_layer : available)
                {
                    if (strcmp(available_layer.layerName, layer) == 0)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }

            return true;
        };

        for (auto& validationLayers : validationLayerPriorityList)
        {
            if (ValidateLayers(validationLayers, layerProperties))
            {
                return validationLayers;
            }
        }

        return {};
    }

    static uint64_t STATIC_COOKIE = 0;
}

DeviceVulkan::DeviceVulkan(GLFWwindow* window, bool debugLayer) :
    mIsDebugLayer(debugLayer),
    mWindow(window)
{
    VkResult res = volkInitialize();
    assert(res == VK_SUCCESS);

    // app info
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Cjing3D App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Cjing3D Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    // enum available layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availablelayerProperties(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availablelayerProperties.data());

    // enum available extension
    uint32_t extensionCount = 0;
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> availableInstanceExtensions(extensionCount);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableInstanceExtensions.data());
    assert(res == VK_SUCCESS);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // enum extension names
    std::vector<const char*> instanceExtensions = GetRequiredExtensions();
    //for (auto& instanceExtension : availableInstanceExtensions)
    //{
    //    if (strcmp(instanceExtension.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
    //    {
    //        mIsDebugUtils = true;
    //        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    //    }
    //    else if (strcmp(instanceExtension.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0)
    //    {
    //        instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    //    }
    //}
    //instanceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // check features by instance extensions
    auto itr = std::find_if(instanceExtensions.begin(), instanceExtensions.end(), [](const char* name) {
        return strcmp(name, VK_KHR_SURFACE_EXTENSION_NAME) == 0;
        });
    bool has_surface_extension = itr != instanceExtensions.end();
    if (has_surface_extension && CheckExtensionSupport(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, availableInstanceExtensions))
    {
        instanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
        mExtensionFeatures.SupportsSurfaceCapabilities2 = true;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // enum layout instances
    std::vector<const char*> instanceLayers;
    if (debugLayer)
    {
        std::vector<const char*> optimalValidationLyers = GetOptimalValidationLayers(availablelayerProperties);
        instanceLayers.insert(instanceLayers.end(), optimalValidationLyers.begin(), optimalValidationLyers.end());
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // inst create info
    VkInstanceCreateInfo instCreateInfo = {};
    instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instCreateInfo.pApplicationInfo = &appInfo;
    instCreateInfo.enabledLayerCount = (uint32_t)instanceLayers.size();
    instCreateInfo.ppEnabledLayerNames = instanceLayers.data();
    instCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

    // debug info
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    if (debugLayer && mIsDebugUtils)
    {
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = DebugCallback;
        instCreateInfo.pNext = &debugUtilsCreateInfo;
    }

    // create instance
    VkResult ret = vkCreateInstance(&instCreateInfo, nullptr, &mVkInstanc);
    if (ret != VK_SUCCESS)
    {
        std::cout << "Failed to create vkInstance." << std::endl;
        return;
    }

    // volk load instance
    volkLoadInstanceOnly(mVkInstanc);

    // create debug utils messager ext
    if (debugLayer && mIsDebugUtils)
    {
        res = vkCreateDebugUtilsMessengerEXT(mVkInstanc, &debugUtilsCreateInfo, nullptr, &mDebugUtilsMessenger);
        assert(res == VK_SUCCESS);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // enum physical devices
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,                // swapchain
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,        // depth clip
    };
    std::vector<const char*> enabledDeviceExtensions;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mVkInstanc, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        std::cout << "Failed to find gpus with vulkan support." << std::endl;
        return;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mVkInstanc, &deviceCount, devices.data());

    bool isBreak = false;
    for (const auto& device : devices)
    {
        if (CheckPhysicalSuitable(device, isBreak))
        {
            enabledDeviceExtensions = requiredDeviceExtensions;

            mPhysicalDevice = device;
            if (isBreak)
                break;
        }
    }

    if (mPhysicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Failed to find a suitable GPU" << std::endl;
        return;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // create surface
    mSurface = CreateSurface(mVkInstanc, mPhysicalDevice);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // find queue families(Graphics, CopyFamily, Compute)
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, nullptr);

    mQueueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyCount, mQueueFamilies.data());

    for (int i = 0; i < (int)QUEUE_INDEX_COUNT; i++)
        mQueueIndices[i] = -1;

    int familyIndex = 0;
    for (const auto& queueFamily : mQueueFamilies)
    {
        if (mQueueIndices[QUEUE_INDEX_GRAPHICS] < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            mQueueIndices[QUEUE_INDEX_GRAPHICS] = familyIndex;

        if (mQueueIndices[QUEUE_INDEX_TRANSFER] < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            mQueueIndices[QUEUE_INDEX_TRANSFER] = familyIndex;

        if (mQueueIndices[QUEUE_INDEX_COMPUTE] < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            mQueueIndices[QUEUE_INDEX_COMPUTE] = familyIndex;

        familyIndex++;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    // create logical device

    // setup queue create infos
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> uniqueQueueFamilies = { 
        mQueueIndices[QUEUE_INDEX_GRAPHICS], 
        mQueueIndices[QUEUE_INDEX_COMPUTE], 
        mQueueIndices[QUEUE_INDEX_GRAPHICS] 
    };
    float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // create device
    VkDeviceCreateInfo createDeviceInfo = {};
    createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createDeviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
    createDeviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    createDeviceInfo.pEnabledFeatures = nullptr;
    createDeviceInfo.pNext = &mFeatures2;
    createDeviceInfo.enabledExtensionCount = (uint32_t)enabledDeviceExtensions.size();
    createDeviceInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();

    res = vkCreateDevice(mPhysicalDevice, &createDeviceInfo, nullptr, &mDevice);
    assert(res == VK_SUCCESS);

    volkLoadDevice(mDevice);

    // get deivce queues
    for (int i = 0; i < QUEUE_INDEX_COUNT; i++)
    {
        if (mQueueIndices[i] != -1)
        {
            vkGetDeviceQueue(mDevice, mQueueIndices[i], 0, &mQueues[i]);
        }
    }


    ///////////////////////////////////////////////////////////////////////////////////////////
    // create frame resources
    const int FRAME_COUNT = 2;
    const int THREAD_COUNT = 1;
    for (int frameIndex = 0; frameIndex < FRAME_COUNT; frameIndex++)
    {

        auto& frameResource = mFrameResources.emplace_back();
        for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
        {
            frameResource.cmdPools->reserve(THREAD_COUNT);
            for (int threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
            {
                frameResource.cmdPools->emplace_back(this, queueIndex);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    //init managers
    mFencePoolManager.Initialize(*this);
    mSemaphoreManager.Initialize(*this);
}

DeviceVulkan::~DeviceVulkan()
{
    mFencePoolManager.ClearAll();
    mSemaphoreManager.ClearAll();

    if (mDebugUtilsMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(mVkInstanc, mDebugUtilsMessenger, nullptr);
    }

    vkDestroySurfaceKHR(mVkInstanc, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroyInstance(mVkInstanc, nullptr);
}

bool DeviceVulkan::CreateSwapchain(Swapchain*& swapchain, uint32_t width, uint32_t height)
{
    if (mSurface == VK_NULL_HANDLE)
        return false;

    if (swapchain != nullptr)
        delete swapchain;

    swapchain = new Swapchain(mDevice);

    // ??????????
    bool useSurfaceInfo = false; // mExtensionFeatures.SupportsSurfaceCapabilities2;
    if (useSurfaceInfo)
    {
    }
    else
    {
        VkSurfaceCapabilitiesKHR surfaceProperties = {};
        auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceProperties);
        assert(res == VK_SUCCESS);

        // get surface formats
        uint32_t formatCount;
        res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr);
        assert(res == VK_SUCCESS);

        if (formatCount != 0)
        {
            swapchain->mFormats.resize(formatCount);
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, swapchain->mFormats.data());
            assert(res == VK_SUCCESS);
        }

        // get present mode
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            swapchain->mPresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, swapchain->mPresentModes.data());
        }

        // find suitable surface format
        VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED };
        VkFormat targetFormat = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
        for (auto& format : swapchain->mFormats)
        {
            if (format.format = targetFormat)
            {
                surfaceFormat = format;
                break;
            }
        }

        if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            std::cout << "Failed to find suitable surface format." << std::endl;
            return false;
        }
        swapchain->mFormat = surfaceFormat;

        // find suitable present mode
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // 交换链是个队列，显示的时候从队列头拿一个图像，程序插入渲染的图像到队列尾。
        bool isVsync = true;                                              // 如果队列满了程序就要等待，这差不多像是垂直同步，显示刷新的时刻就是垂直空白
        if (!isVsync)
        {
            for (auto& presentMode : swapchain->mPresentModes)
            {
                if (presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    swapchainPresentMode = presentMode;
                    break;
                }
            }
        }

        // set swapchin extent
        swapchain->mSwapchainExtent = { width, height };
        swapchain->mSwapchainExtent.width =
            max(surfaceProperties.minImageExtent.width, min(surfaceProperties.maxImageExtent.width, width));
        swapchain->mSwapchainExtent.height =
            max(surfaceProperties.minImageExtent.height, min(surfaceProperties.maxImageExtent.height, height));

        // create swapchain
        uint32_t desiredSwapchainImages = 2;
        if (desiredSwapchainImages < surfaceProperties.minImageCount)
            desiredSwapchainImages = surfaceProperties.minImageCount;
        if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
            desiredSwapchainImages = surfaceProperties.maxImageCount;


        VkSwapchainKHR oldSwapchain = swapchain->mSwapChain;;
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = mSurface;
        createInfo.minImageCount = desiredSwapchainImages;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = swapchain->mSwapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;            // 一个图像在某个时间点就只能被一个队列族占用，在被另一个队列族使用前，它的占用情况一定要显式地进行转移。该选择提供了最好的性能。
        createInfo.preTransform = surfaceProperties.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;      // 该alpha通道是否应该和窗口系统中的其他窗口进行混合, 默认不混合
        createInfo.presentMode = swapchainPresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        res = vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &swapchain->mSwapChain);
        assert(res == VK_SUCCESS);

        uint32_t imageCount = 0;
        res = vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapChain, &imageCount, nullptr);
        assert(res == VK_SUCCESS);

        std::vector<VkImage> images(imageCount);
        res = vkGetSwapchainImagesKHR(mDevice, swapchain->mSwapChain, &imageCount, images.data());
        assert(res == VK_SUCCESS);

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // create image views
        ImageCreateInfo imageCreateInfo = ImageCreateInfo::RenderTarget(width, height, swapchain->mFormat.format);
        for (int i = 0; i < images.size(); i++)
        {
            VkImageView imageView;
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchain->mFormat.format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            res = vkCreateImageView(mDevice, &createInfo, nullptr, &imageView);
            assert(res == VK_SUCCESS);

            ImagePtr imagePtr = ImagePtr(mImagePool.allocate(*this, images[i], imageView, imageCreateInfo));
            if (imagePtr)
            {
                imagePtr->DisownImge(); // image由vkGetSwapchainImagesKHR获取
                imagePtr->SetSwapchainLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                swapchain->mImages.push_back(imagePtr);
            }

            // 在GetSwapchainRenderPassInfo中根据Hash获取
            //VkImageView attachments[] = {
            //    swapchain->mImageViews[i]
            //};
            //// 创建frameBuffer与ImageView一一对应
            //VkFramebufferCreateInfo framebufferInfo = {};
            //res = vkCreateFramebuffer(mDevice, &framebufferInfo, nullptr, &swapchain->mFrameBuffers[i]);
            //assert(res == VK_SUCCESS);
        }
    }

    return true;
}

Util::IntrusivePtr<CommandList> DeviceVulkan::RequestCommandList(QueueType queueType)
{
    return RequestCommandList(0, queueType);
}

Util::IntrusivePtr<CommandList> DeviceVulkan::RequestCommandList(int threadIndex, QueueType queueType)
{
    // Only support main thread now
    auto& pools = CurrentFrameResource().cmdPools[(int)queueType];
    assert(threadIndex == 0 && threadIndex < pools.size());

    CommandPool& pool = pools[threadIndex];
    VkCommandBuffer buffer = pool.RequestCommandBuffer();
    if (buffer == VK_NULL_HANDLE)
        return Util::IntrusivePtr<CommandList>();

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult res = vkBeginCommandBuffer(buffer, &info);
    assert(res == VK_SUCCESS);

    return Util::IntrusivePtr<CommandList>(mCommandListPool.allocate(*this, buffer, queueType));
}

RenderPassInfo DeviceVulkan::GetSwapchianRenderPassInfo(const Swapchain& swapChain, SwapchainRenderPassType swapchainRenderPassType)
{
    RenderPassInfo info = {};
    info.mNumColorAttachments = 1;
    info.mColorAttachments[0] = &swapChain.mImages[swapChain.mImageIndex]->GetImageView();
    info.mClearAttachments = ~0u;
    info.mStoreAttachments = 1u << 0;

    return info;
}

RenderPass& DeviceVulkan::RequestRenderPass(const RenderPassInfo& renderPassInfo)
{
    HashCombiner hash;
    VkFormat colorFormats[VULKAN_NUM_ATTACHMENTS];
    VkFormat depthStencilFormat = VkFormat::VK_FORMAT_UNDEFINED;

    // Color attachments
    for (uint32_t i = 0; i < renderPassInfo.mNumColorAttachments; i++)
    {
        const ImageView& imageView = *renderPassInfo.mColorAttachments[i];
        colorFormats[i] = imageView.GetInfo().mFormat;
        hash.HashCombine(imageView.GetImage()->GetSwapchainLayout());
    }

    // Depth stencil
    if (renderPassInfo.mDepthStencil)
        depthStencilFormat = renderPassInfo.mDepthStencil->GetInfo().mFormat;

    // Subpasses
    hash.HashCombine(renderPassInfo.mNumSubPasses);
    for (uint32_t i = 0; i < renderPassInfo.mNumSubPasses; i++)
    {
        auto& subpassInfo = renderPassInfo.mSubPasses[i];
        hash.HashCombine(subpassInfo.mNumColorAattachments);
        hash.HashCombine(subpassInfo.mNumInputAttachments);
        hash.HashCombine(subpassInfo.mNumResolveAttachments);
		for (uint32_t j = 0; j < subpassInfo.mNumColorAattachments; j++)
            hash.HashCombine(subpassInfo.mColorAttachments[j]);
		for (uint32_t j = 0; j < subpassInfo.mNumInputAttachments; j++)
            hash.HashCombine(subpassInfo.mInputAttachments[j]);
		for (uint32_t j = 0; j < subpassInfo.mNumResolveAttachments; j++)
            hash.HashCombine(subpassInfo.mResolveAttachments[j]);
    }

    // Calculate hash
	for (uint32_t i = 0; i < renderPassInfo.mNumColorAttachments; i++)
        hash.HashCombine((uint32_t)colorFormats[i]);

    hash.HashCombine(renderPassInfo.mNumColorAttachments);
    hash.HashCombine((uint32_t)depthStencilFormat);

    auto findIt = mRenderPasses.find(hash.Get());
    if (findIt != mRenderPasses.end())
        return findIt->second;

    RenderPass& renderPass = mRenderPasses.try_emplace(hash.Get(), *this, renderPassInfo).first->second;
    renderPass.SetHash(hash.Get());
    return renderPass;
}

FrameBuffer& DeviceVulkan::RequestFrameBuffer(const RenderPassInfo& renderPassInfo)
{
    HashCombiner hash;

    // 需要根据RenderPass以及RT的Hash值获取或者创建FrameBuffer
    RenderPass& renderPass = RequestRenderPass(renderPassInfo);
    hash.HashCombine(renderPass.GetHash());

    // Get color attachments hash
    for (uint32_t i = 0; i < renderPassInfo.mNumColorAttachments; i++)
        hash.HashCombine(renderPassInfo.mColorAttachments[i]->GetCookie());

    // Get depth stencil hash
    if (renderPassInfo.mDepthStencil)
        hash.HashCombine(renderPassInfo.mDepthStencil->GetCookie());

    auto findIt = mFrameBuffers.find(hash.Get());
    if (findIt != mFrameBuffers.end())
        return findIt->second;

    return mFrameBuffers.try_emplace(hash.Get(), *this, renderPass, renderPassInfo).first->second;
}

void DeviceVulkan::BeginFrameContext()
{
    // submit remain queue
    EndFrameContext();

    if (++mFrameIndex >= mFrameResources.size())
        mFrameIndex = 0;

    // reset recyle fences
    auto& recyleFences = CurrentFrameResource().mRecyleFences;
    if (!recyleFences.empty())
    {
        vkResetFences(mDevice, (uint32_t)recyleFences.size(), recyleFences.data());
        for (auto& fence : recyleFences)
            mFencePoolManager.Recyle(fence);
        recyleFences.clear();
    }

    // begin frame resources
    CurrentFrameResource().Begin(mDevice);

    // clear destroyed resources
    CurrentFrameResource().ProcessDestroyed(mDevice);

    // reset recyle semaphores
    auto& recyleSemaphores = CurrentFrameResource().mRecycledSemaphroes;
    for (auto& semaphore : recyleSemaphores)
        mSemaphoreManager.Recyle(semaphore);
    recyleSemaphores.clear();
}

void DeviceVulkan::EndFrameContext()
{
    // flush queue
    InternalFence fence;
    auto& frame = CurrentFrameResource();
    auto submissionList = frame.mSubmissions;
    for (auto& queueIndex : QUEUE_FLUSH_ORDER)
    {
        if (!submissionList[queueIndex].empty())
        {
            SubmitQueue(queueIndex, &fence);
           
            // 如果创建了Fence，则添加到waitFences，这会在BeginFrame前等待
            if (fence.mFence != VK_NULL_HANDLE)
            {
                frame.mWaitFences.push_back(fence.mFence);
                frame.mRecyleFences.push_back(fence.mFence);
            }
        }
    }
}

void DeviceVulkan::Submit(CommandListPtr& cmd)
{
    cmd->EndCommandBuffer();

    QueueIndices queueIndex = ConvertQueueTypeToIndices(cmd->GetQueueType());
    auto& submissions = CurrentFrameResource().mSubmissions[queueIndex];
    submissions.push_back(std::move(cmd));
}

void DeviceVulkan::SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire)
{
    mWSI.mAcquire = std::move(acquire);
    mWSI.mIndex = index;
    mWSI.mConsumed = false;
}

SemaphorePtr DeviceVulkan::RequestSemaphore()
{
    VkSemaphore semaphore = mSemaphoreManager.Requset();
    return SemaphorePtr(mSemaphorePool.allocate(*this, semaphore, false));
}

void DeviceVulkan::ReleaseFrameBuffer(VkFramebuffer buffer)
{
    CurrentFrameResource().mDestroyedFrameBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseImage(VkImage image)
{
    CurrentFrameResource().mDestroyedImages.push_back(image);
}

void DeviceVulkan::ReleaseImageView(VkImageView imageView)
{
    CurrentFrameResource().mDestroyedImageViews.push_back(imageView);
}

void DeviceVulkan::ReleaseFence(VkFence fence, bool isWait)
{
    if (isWait)
    {
        vkResetFences(mDevice, 1, &fence);
        mFencePoolManager.Recyle(fence);
    }
    else
    {
        CurrentFrameResource().mRecyleFences.push_back(fence);
    }
}

void DeviceVulkan::ReleaseSemaphore(VkSemaphore semaphore, bool isSignalled)
{
    // 已经signalled的semaphore则直接销毁，否则循环使用
    if (isSignalled)
    {
        CurrentFrameResource().mDestroyeSemaphores.push_back(semaphore);
    }
    else
    {
        CurrentFrameResource().mRecycledSemaphroes.push_back(semaphore);
    }
}

uint64_t DeviceVulkan::GenerateCookie()
{
    STATIC_COOKIE += 16;
    return STATIC_COOKIE;
}

SemaphorePtr DeviceVulkan::GetAndConsumeReleaseSemaphore()
{
    auto ret = std::move(mWSI.mRelease);
    mWSI.mRelease.reset();
    return ret;
}

VkQueue DeviceVulkan::GetPresentQueue() const
{
    return mWSI.mPresentQueue;
}

bool DeviceVulkan::CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak)
{
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME,
    };

    // 1. check device support required device extensions
    uint32_t extensionCount;
    VkResult res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    assert(res == VK_SUCCESS);
    std::vector<VkExtensionProperties> availableDeviceExtensions(extensionCount);
    res = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableDeviceExtensions.data());
    assert(res == VK_SUCCESS);

    for (auto& requiredExt : requiredDeviceExtensions)
    {
        if (!CheckExtensionSupport(requiredExt, availableDeviceExtensions))
            return false;
    }

    // 2. check device properties
    bool ret = mPhysicalDevice == VK_NULL_HANDLE;
    vkGetPhysicalDeviceProperties2(device, &mProperties2);
    if (mProperties2.properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        isBreak = true;
        ret = true;
    }

    // 3. check device features
    mFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(device, &mFeatures2);

    ret &= mFeatures2.features.imageCubeArray == VK_TRUE;
    ret &= mFeatures2.features.independentBlend == VK_TRUE;
    ret &= mFeatures2.features.geometryShader == VK_TRUE;
    ret &= mFeatures2.features.samplerAnisotropy == VK_TRUE;
    ret &= mFeatures2.features.shaderClipDistance == VK_TRUE;
    ret &= mFeatures2.features.textureCompressionBC == VK_TRUE;
    ret &= mFeatures2.features.occlusionQueryPrecise == VK_TRUE;

    return ret;
}

bool DeviceVulkan::CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions)
{
    for (const auto& x : availableExtensions)
    {
        if (strcmp(x.extensionName, checkExtension) == 0)
            return true;
    }
    return false;
}

VkSurfaceKHR DeviceVulkan::CreateSurface(VkInstance instance, VkPhysicalDevice)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, mWindow, nullptr, &surface) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    int actualWidth, actualHeight;
    glfwGetFramebufferSize(mWindow, &actualWidth, &actualHeight);
    mWidth = unsigned(actualWidth);
    mHeight = unsigned(actualHeight);
    return surface;
}

std::vector<const char*> DeviceVulkan::GetRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (mIsDebugUtils) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

void DeviceVulkan::BakeShaderProgram(ShaderProgram& program)
{
}

void DeviceVulkan::SubmitQueue(QueueIndices queueIndex, InternalFence* fence)
{
    auto& submissions = CurrentFrameResource().mSubmissions[queueIndex];

    // 如果提交队列为空，则直接空提交
    if (submissions.empty())
    {
        if (fence != nullptr)
            SubmitEmpty(queueIndex, fence);
        return;
    }

    VkQueue queue = mQueues[queueIndex];
    BatchComposer batchComposer;
    for (int i = 0; i < submissions.size(); i++)
    {
        auto& cmd = submissions[i];
        VkPipelineStageFlags stages = cmd->GetSwapchainStages();  

        // use swapchian in stages
        if (stages != 0 && !mWSI.mConsumed)
        {
            // Acquire semaphore
            if (mWSI.mAcquire && mWSI.mAcquire->GetSemaphore() != VK_NULL_HANDLE)
            {
                batchComposer.AddWaitSemaphore(mWSI.mAcquire, stages);
                if (mWSI.mAcquire->GetTimeLine() <= 0)
                {
                    ReleaseSemaphore(mWSI.mAcquire->GetSemaphore(), mWSI.mAcquire->IsSignalled());
                }

                mWSI.mAcquire->Release();
                mWSI.mAcquire.reset();
            }

            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());

            // Release semaphore
            VkSemaphore release = mSemaphoreManager.Requset();
            mWSI.mRelease = SemaphorePtr(mSemaphorePool.allocate(*this, release, true));
            batchComposer.AddSignalSemaphore(release);

            mWSI.mPresentQueue = queue;
            mWSI.mConsumed = true;
        }
        else
        {
            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());
        }
    }

    VkFence clearedFence = fence ? mFencePoolManager.Requset() : VK_NULL_HANDLE;
    if (fence)
        fence->mFence = clearedFence;

    VkResult ret = SubmitBatches(batchComposer, queue, clearedFence);
    if (ret != VK_SUCCESS)
    {
        std::cout << "Submit queue failed: " << int(ret) << std::endl;
        return;
    }

    submissions.clear();
}

void DeviceVulkan::SubmitEmpty(QueueIndices queueIndex, InternalFence* fence)
{
}

VkResult DeviceVulkan::SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence)
{
    auto& submits = composer.Bake();
    return vkQueueSubmit(queue, (uint32_t)submits.size(), submits.data(), fence);
}

bool DeviceVulkan::CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader)
{
    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = bytecodeLength;
    moduleInfo.pCode = (const uint32_t*)pShaderBytecode;
    VkResult res = vkCreateShaderModule(mDevice, &moduleInfo, nullptr, &shader->mShaderModule);
    return res == VK_SUCCESS;
}

void DeviceVulkan::FrameResource::Begin(VkDevice device)
{
    // wait for submiting
    if (!mWaitFences.empty())
    {
        vkWaitForFences(device, (uint32_t)mWaitFences.size(), mWaitFences.data(), VK_TRUE, UINT64_MAX);
        mWaitFences.clear();
    }

    // reset command pools
    for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
    {
        for (auto& pool : cmdPools[queueIndex])
            pool.BeginFrame();
    }
}

void DeviceVulkan::FrameResource::ProcessDestroyed(VkDevice device)
{
	for (auto& buffer : mDestroyedFrameBuffers)
		vkDestroyFramebuffer(device, buffer, nullptr);
    for (auto& image : mDestroyedImages)
        vkDestroyImage(device, image, nullptr);
    for (auto& imageView : mDestroyedImageViews)
		vkDestroyImageView(device, imageView, nullptr);
    for (auto& semaphore : mDestroyeSemaphores)
        vkDestroySemaphore(device, semaphore, nullptr);

	mDestroyedFrameBuffers.clear();
	mDestroyedImages.clear();
	mDestroyedImageViews.clear();
    mDestroyeSemaphores.clear();
}

BatchComposer::BatchComposer()
{
    // 默认存在一个Submit
    mSubmits.emplace_back();
}

void BatchComposer::BeginBatch()
{
    // create a new VkSubmitInfo
    auto& submitInfo = mSubmitInfos[mSubmitIndex];
    if (!submitInfo.mCommandLists.empty() || !submitInfo.mWaitSemaphores.empty())
    {
        mSubmitIndex = (uint32_t)mSubmits.size();
        mSubmits.emplace_back();
    }
}

void BatchComposer::AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages)
{
    // 添加wait semaphores时先将之前的cmds batch
    if (!mSubmitInfos[mSubmitIndex].mCommandLists.empty())
        BeginBatch();

    mSubmitInfos[mSubmitIndex].mWaitSemaphores.push_back(sem->GetSemaphore());
    mSubmitInfos[mSubmitIndex].mWaitStages.push_back(stages);
}

void BatchComposer::AddSignalSemaphore(VkSemaphore sem)
{
    mSubmitInfos[mSubmitIndex].mSignalSemaphores.push_back(sem);
}

void BatchComposer::AddCommandBuffer(VkCommandBuffer buffer)
{
    // 如果存在Signal semaphores，先将之前的cmds batch
    if (!mSubmitInfos[mSubmitIndex].mSignalSemaphores.empty())
        BeginBatch();

    mSubmitInfos[mSubmitIndex].mCommandLists.push_back(buffer);
}

std::vector<VkSubmitInfo>& BatchComposer::Bake()
{
    // setup VKSubmitInfos
    for (size_t index = 0; index < mSubmits.size(); index++)
    {
        auto& vkSubmitInfo = mSubmits[index];
        auto& submitInfo = mSubmitInfos[index];
        vkSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        vkSubmitInfo.commandBufferCount = (uint32_t)submitInfo.mCommandLists.size();
        vkSubmitInfo.pCommandBuffers = submitInfo.mCommandLists.data();

        vkSubmitInfo.waitSemaphoreCount = (uint32_t)submitInfo.mWaitSemaphores.size();
        vkSubmitInfo.pWaitSemaphores = submitInfo.mWaitSemaphores.data();
        vkSubmitInfo.pWaitDstStageMask = submitInfo.mWaitStages.data();

        vkSubmitInfo.signalSemaphoreCount = (uint32_t)submitInfo.mSignalSemaphores.size();
        vkSubmitInfo.pSignalSemaphores = submitInfo.mSignalSemaphores.data();
    }

    return mSubmits;
}
