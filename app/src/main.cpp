

// platform win32
#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#include <atlbase.h> // ComPtr

#define CiLoadLibrary(name) LoadLibrary(_T(name))
#define CiGetProcAddress(handle,name) GetProcAddress(handle, name)

#define VK_USE_PLATFORM_WIN32_KHR
#endif

// vulkan
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

#include <iostream>
#include <assert.h>
#include <vector>
#include <set>
#include <math.h>

#ifdef DEBUG
bool debugLayer = true;
#else
bool debugLayer = false;
#endif

#include "dxcompiler\inc\d3d12shader.h"
#include "dxcompiler\inc\dxcapi.h"

namespace {
    CComPtr<IDxcCompiler3> dxcCompiler;
}

struct RenderPass
{
    VkRenderPass mRenderPass = VK_NULL_HANDLE;;
};

struct CommandList
{
    VkDevice& mDevice;
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    RenderPass* mRenderPass = nullptr;
    bool mIsDirtyPipeline = true;

    CommandList(VkDevice& device) : mDevice(device)
    {
    }

    ~CommandList()
    {
        if (mCurrentPipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(mDevice, mCurrentPipeline, nullptr);
    }

    bool FlushRenderState()
    {
        if (mCurrentPipeline == VK_NULL_HANDLE)
            mIsDirtyPipeline = true;

        if (mIsDirtyPipeline)
        {
            mIsDirtyPipeline = false;
            VkPipeline oldPipeline = mCurrentPipeline;
            if (!FlushGraphicsPipeline())
                return false;

            if (oldPipeline != mCurrentPipeline)
            {
                // bind pipeline
                //vkCmdBindPipeline()
            }
        }

        return true;
    }

    bool FlushGraphicsPipeline()
    {
        mCurrentPipeline = BuildGraphicsPipeline();
        return mCurrentPipeline != VK_NULL_HANDLE;
    }

    VkPipeline BuildGraphicsPipeline()
    {
        /////////////////////////////////////////////////////////////////////////////////////////
        // view port
        mViewport.x = 0;
        mViewport.y = 0;
        mViewport.width = 65535;
        mViewport.height = 65535;
        mViewport.minDepth = 0;
        mViewport.maxDepth = 1;

        mScissor.extent.width = 65535;
        mScissor.extent.height = 65535;

        VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.pViewports = &mViewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &mScissor;

        /////////////////////////////////////////////////////////////////////////////////////////
        // dynamic state

        /////////////////////////////////////////////////////////////////////////////////////////
        // blend state

        /////////////////////////////////////////////////////////////////////////////////////////
        // depth state

        /////////////////////////////////////////////////////////////////////////////////////////
        // vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;

        // attributes
        VkVertexInputAttributeDescription& attr = attributes.emplace_back();
        attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr.location = 0;
        attr.offset = 0;

        // bindings
        VkVertexInputBindingDescription& bind = bindings.emplace_back();
        bind.binding = 0;
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bind.stride = 0;

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
        vertexInputInfo.pVertexBindingDescriptions = bindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

        // input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /////////////////////////////////////////////////////////////////////////////////////////
        // raster

        /////////////////////////////////////////////////////////////////////////////////////////
        // stages

        VkPipeline retPipeline = VK_NULL_HANDLE;
        VkGraphicsPipelineCreateInfo pipeCreateInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
            

        VkResult res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipeCreateInfo, nullptr, &retPipeline);
        if (res != VK_SUCCESS)
        {
            std::cout << "Failed to create graphics pipeline!" << std::endl;
            return VK_NULL_HANDLE;
        }
        return retPipeline;
    }
};

struct DeviceFeatures
{
    bool SupportsSurfaceCapabilities2 = false;
};

struct Swapchain
{
    VkDevice& mDevice;
    VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR mFormat;
    VkExtent2D mSwapchainExtent;
    std::vector<VkImage> mImages;
    std::vector<VkImageView> mImageViews;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;
    RenderPass mDefaultRenderPass;
    VkSemaphore mSwapchainAcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore mSwapchainReleaseSemaphore = VK_NULL_HANDLE;

    Swapchain(VkDevice& device) : mDevice(device)
    {
    }

    ~Swapchain()
    {
        for (int i = 0; i < mImageViews.size(); i++)
        {
            vkDestroyImageView(mDevice, mImageViews[i], nullptr);
        }

        vkDestroySemaphore(mDevice, mSwapchainAcquireSemaphore, nullptr);
        vkDestroySemaphore(mDevice, mSwapchainReleaseSemaphore, nullptr);
        vkDestroyRenderPass(mDevice, mDefaultRenderPass.mRenderPass, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    }
};

class DeviceVulkan
{
private:
    GLFWwindow* mWindow;
    uint32_t mWidth = 0.0f;
    uint32_t mHeight = 0.0f;

    VkDevice mDevice;
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};

    DeviceFeatures mExtensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    int mGraphicsFamily = -1;
    int mComputeFamily = -1;
    int mCopyFamily = -1;
    VkQueue mgGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mCopyQueue = VK_NULL_HANDLE;

    VkSurfaceKHR mSurface;
    Swapchain* mSwapchain = nullptr;

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer) :
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
    
        int familyIndex = 0;
        for (const auto& queueFamily : mQueueFamilies)
        {
            if (mGraphicsFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                mGraphicsFamily = familyIndex;
            }

            if (mCopyFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            {
                mCopyFamily = familyIndex;
            }

            if (mComputeFamily < 0 && queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                mComputeFamily = familyIndex;
            }

            familyIndex++;
        }

        ///////////////////////////////////////////////////////////////////////////////////////////
        // create logical device

        // setup queue create infos
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueQueueFamilies = { mGraphicsFamily, mCopyFamily, mComputeFamily };
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
        vkGetDeviceQueue(mDevice, mGraphicsFamily, 0, &mgGraphicsQueue);
        vkGetDeviceQueue(mDevice, mComputeFamily, 0, &mComputeQueue);
        vkGetDeviceQueue(mDevice, mCopyFamily, 0, &mCopyQueue);

        ///////////////////////////////////////////////////////////////////////////////////////////
        // init swapchian
        if (!InitSwapchain(mPhysicalDevice, mWidth, mHeight))
        {
            std::cout << "Failed to init swapchian." << std::endl;
            return;
        }
    }

    ~DeviceVulkan()
    {
        if (mDebugUtilsMessenger != VK_NULL_HANDLE) {
            vkDestroyDebugUtilsMessengerEXT(mVkInstanc, mDebugUtilsMessenger, nullptr);
        }

        if (mSwapchain != nullptr)
            delete mSwapchain;

        vkDestroySurfaceKHR(mVkInstanc, mSurface, nullptr);
        vkDestroyDevice(mDevice, nullptr);
        vkDestroyInstance(mVkInstanc, nullptr);
    }

private:
    bool InitSwapchain(VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height)
    {
        if (mSurface == VK_NULL_HANDLE)
            return false;

        mSwapchain = new Swapchain(mDevice);

        // ??????????
        bool useSurfaceInfo = false; // mExtensionFeatures.SupportsSurfaceCapabilities2;
        if (useSurfaceInfo)
        {
        }
        else
        {
            VkSurfaceCapabilitiesKHR surfaceProperties = {};
            auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, mSurface, &surfaceProperties);
            assert(res == VK_SUCCESS);

            // get surface formats
            uint32_t formatCount;
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, nullptr);
            assert(res == VK_SUCCESS);

            if (formatCount != 0)
            {
                mSwapchain->mFormats.resize(formatCount);
                res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, mSurface, &formatCount, mSwapchain->mFormats.data());
                assert(res == VK_SUCCESS);
            }

            // get present mode
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, nullptr);

            if (presentModeCount != 0) 
            {
                mSwapchain->mPresentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, mSurface, &presentModeCount, mSwapchain->mPresentModes.data());
            }

            // find suitable surface format
            VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED };
            VkFormat targetFormat = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
            for (auto& format : mSwapchain->mFormats)
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
            mSwapchain->mFormat = surfaceFormat;

            // find suitable present mode
            VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // 交换链是个队列，显示的时候从队列头拿一个图像，程序插入渲染的图像到队列尾。
            bool isVsync = true;                                              // 如果队列满了程序就要等待，这差不多像是垂直同步，显示刷新的时刻就是垂直空白
            if (!isVsync)
            {
                for (auto& presentMode : mSwapchain->mPresentModes)
                {
                    if (presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
                    {
                        swapchainPresentMode = presentMode;
                        break;
                    }
                }
            }
           
            // set swapchin extent
            mSwapchain->mSwapchainExtent = { width, height };
            mSwapchain->mSwapchainExtent.width = 
                max(surfaceProperties.minImageExtent.width, min(surfaceProperties.maxImageExtent.width, width));
            mSwapchain->mSwapchainExtent.height =
                max(surfaceProperties.minImageExtent.height, min(surfaceProperties.maxImageExtent.height, height));

            // create swapchain
            int desiredSwapchainImages = 2;
            if (desiredSwapchainImages < surfaceProperties.minImageCount)
                desiredSwapchainImages = surfaceProperties.minImageCount;
            if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
                desiredSwapchainImages = surfaceProperties.maxImageCount;


            VkSwapchainKHR oldSwapchain = mSwapchain->mSwapChain;;
            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = mSurface;
            createInfo.minImageCount = desiredSwapchainImages;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = mSwapchain->mSwapchainExtent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;            // 一个图像在某个时间点就只能被一个队列族占用，在被另一个队列族使用前，它的占用情况一定要显式地进行转移。该选择提供了最好的性能。
            createInfo.preTransform = surfaceProperties.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;      // 该alpha通道是否应该和窗口系统中的其他窗口进行混合, 默认不混合
            createInfo.presentMode = swapchainPresentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = oldSwapchain;

            res = vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain->mSwapChain);
            assert(res == VK_SUCCESS);

            uint32_t imageCount = 0;
            res = vkGetSwapchainImagesKHR(mDevice, mSwapchain->mSwapChain, &imageCount, nullptr);
            assert(res == VK_SUCCESS);
            mSwapchain->mImages.resize(imageCount);
            res = vkGetSwapchainImagesKHR(mDevice, mSwapchain->mSwapChain, &imageCount, mSwapchain->mImages.data());
            assert(res == VK_SUCCESS);

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // create default render pass
            // 对应于Swapchain image的颜色缓冲附件
            VkAttachmentDescription colorAttachment = {};
            colorAttachment.format = mSwapchain->mFormat.format;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;

            VkRenderPassCreateInfo createRenderPassInfo = {};
            createRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            createRenderPassInfo.attachmentCount = 1;
            createRenderPassInfo.pAttachments = &colorAttachment;
            createRenderPassInfo.subpassCount = 1;
            createRenderPassInfo.pSubpasses = &subpass;

            res = vkCreateRenderPass(mDevice, &createRenderPassInfo, nullptr, &mSwapchain->mDefaultRenderPass.mRenderPass);
            assert(res == VK_SUCCESS);

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // create image views
            mSwapchain->mImageViews.resize(mSwapchain->mImages.size());
            for (int i = 0; i < mSwapchain->mImages.size(); i++)
            {
                VkImageViewCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createInfo.image = mSwapchain->mImages[i];
                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                createInfo.format = mSwapchain->mFormat.format;
                createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                createInfo.subresourceRange.baseMipLevel = 0;
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1;

                res = vkCreateImageView(mDevice, &createInfo, nullptr, &mSwapchain->mImageViews[i]);
                assert(res == VK_SUCCESS);
            }

            /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
            // create semaphore
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            
            if (mSwapchain->mSwapchainAcquireSemaphore == nullptr)
            {
                res = vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mSwapchain->mSwapchainAcquireSemaphore);
                assert(res == VK_SUCCESS);
            }

            if (mSwapchain->mSwapchainReleaseSemaphore == nullptr)
            {
                res = vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mSwapchain->mSwapchainReleaseSemaphore);
                assert(res == VK_SUCCESS);
            }
        }

        return true;
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

    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak)
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

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions)
    {
        for (const auto& x : availableExtensions)
        {
            if (strcmp(x.extensionName, checkExtension) == 0)
                return true;
        }
        return false;
    }

    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice)
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


    std::vector<const char*> GetRequiredExtensions() 
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
};


int main()
{
    // init dxcompiler
    HMODULE dxcompiler = CiLoadLibrary("dxcompiler.dll");
    if (dxcompiler != nullptr)
    {
        DxcCreateInstanceProc DxcCreateInstance = (DxcCreateInstanceProc)CiGetProcAddress(dxcompiler, "DxcCreateInstance");
        if (DxcCreateInstance != nullptr)
        {
            HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
            assert(SUCCEEDED(hr));
        }
    }

    // init glfw
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    
    // create window
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Test", nullptr, nullptr);

    // init gpu
    DeviceVulkan* deviceVulkan = new DeviceVulkan(window, debugLayer);
    
    // main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    // uninit
    delete deviceVulkan;

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}