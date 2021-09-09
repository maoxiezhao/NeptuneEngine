#pragma once

#include "definition.h"
#include "common.h"
#include "commandList.h"
#include "image.h"
#include "renderPass.h"
#include "fence.h"

#include <set>
#include <unordered_map>

struct DeviceFeatures
{
    bool SupportsSurfaceCapabilities2 = false;
};

struct Swapchain
{
    VkDevice& mDevice;
    VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR mFormat = {};
    VkExtent2D mSwapchainExtent = {};
    std::vector<ImagePtr> mImages;
    std::vector<VkSurfaceFormatKHR> mFormats;
    std::vector<VkPresentModeKHR> mPresentModes;

    uint32_t mImageIndex = 0;
    VkSemaphore mAcquireSemaphore = VK_NULL_HANDLE;
    VkSemaphore mReleaseSemaphore = VK_NULL_HANDLE;

    Swapchain(VkDevice& device) : mDevice(device)
    {
    }

    ~Swapchain()
    {
        vkDestroySemaphore(mDevice, mAcquireSemaphore, nullptr);
        vkDestroySemaphore(mDevice, mReleaseSemaphore, nullptr);
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    }
};

class DeviceVulkan
{
public:
    // base info
    GLFWwindow* mWindow;
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    bool mIsDebugUtils = false;
    bool mIsDebugLayer = false;

    // core 
    VkDevice mDevice;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkInstance mVkInstanc;
    VkDebugUtilsMessengerEXT mDebugUtilsMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface;
    
    // features
    VkPhysicalDeviceProperties2 mProperties2 = {};
    VkPhysicalDeviceVulkan11Properties mProperties_1_1 = {};
    VkPhysicalDeviceVulkan12Properties mProperties_1_2 = {};
    DeviceFeatures mExtensionFeatures = {};
    VkPhysicalDeviceFeatures2 mFeatures2 = {};

    // queue
    std::vector<VkQueueFamilyProperties> mQueueFamilies;
    int mGraphicsFamily = -1;
    int mComputeFamily = -1;
    int mCopyFamily = -1;
    VkQueue mgGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mComputeQueue = VK_NULL_HANDLE;
    VkQueue mCopyQueue = VK_NULL_HANDLE;

    // per frame resource
    struct FrameResource
    {
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

        // destroyed resoruces
        std::vector<VkImageView> mDestroyedImageViews;
        std::vector<VkImage> mDestroyedImages;
        std::vector<VkFramebuffer> mDestroyedFrameBuffers;

        // fences
        std::vector<VkFence> mRecyleFences;

        void Begin();
        void ProcessDestroyed(VkDevice device);
    };
    std::vector<FrameResource> mFrameResources;
    uint32_t mFrameIndex = 0;

    FrameResource& CurrentFrameResource()
    {
        assert(mFrameIndex < mFrameResources.size());
        return mFrameResources[mFrameIndex];
    }

    // rhi object pools
    std::unordered_map<uint64_t, RenderPass> mRenderPasses;
    std::unordered_map<uint64_t, FrameBuffer> mFrameBuffers;

    Util::ObjectPool<CommandList> mCommandListPool;
    Util::ObjectPool<Image> mImagePool;
    Util::ObjectPool<ImageView> mImageViewPool;
    Util::ObjectPool<Fence> mFencePool;

    // vulkan object managers
    FenceManager mFencePoolManager;

public:
    DeviceVulkan(GLFWwindow* window, bool debugLayer);
    ~DeviceVulkan();

    bool CreateSwapchain(Swapchain*& swapchain, uint32_t width, uint32_t height);
    bool CreateShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, Shader* shader);
    
    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandList(int threadIndex, QueueType queueType);
    
    RenderPassInfo GetSwapchianRenderPassInfo(const Swapchain& swapChain, SwapchainRenderPassType swapchainRenderPassType);
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);

    void BeginFrameContext();
    void EndFrameContext();
    void Submit(CommandListPtr& cmd);

    void ReleaseFrameBuffer(VkFramebuffer buffer);
    void ReleaseImage(VkImage image);
    void ReleaseImageView(VkImageView imageView);
    void ReleaseFence(VkFence fence, bool isWait);

    uint64_t GenerateCookie();

private:

    // initial methods
    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak);
    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions);
    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice);
    std::vector<const char*> GetRequiredExtensions();

    void SubmitCmd(CommandListPtr cmd);
    void SubmitQueue();

    // debug callback
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }
};
