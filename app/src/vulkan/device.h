#pragma once

#include "definition.h"
#include "common.h"
#include "commandList.h"
#include "image.h"
#include "renderPass.h"
#include "fence.h"
#include "semaphore.h"

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

    Swapchain(VkDevice& device) : mDevice(device)
    {
    }

    ~Swapchain()
    {
        vkDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
    }
};

struct BatchComposer
{
public:
    BatchComposer();

    static const uint32_t MAX_SUBMIT_COUNT = 8;
    uint32_t mSubmitIndex = 0;
    struct SubmitInfo
    {
        std::vector<VkPipelineStageFlags> mWaitStages;
        std::vector<VkSemaphore> mWaitSemaphores;
        std::vector<VkSemaphore> mSignalSemaphores;
        std::vector<VkCommandBuffer> mCommandLists;
    };
    SubmitInfo mSubmitInfos[MAX_SUBMIT_COUNT];
    std::vector<VkSubmitInfo> mSubmits;

private:
    void BeginBatch();

public:
    void AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages);
    void AddSignalSemaphore(VkSemaphore sem);
    void AddCommandBuffer(VkCommandBuffer buffer);
    std::vector<VkSubmitInfo>& Bake();
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
    int mQueueIndices[QUEUE_INDEX_COUNT] = {};
    VkQueue mQueues[QUEUE_INDEX_COUNT] = {};

    // per frame resource
    struct FrameResource
    {
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

        // destroyed resoruces
        std::vector<VkImageView> mDestroyedImageViews;
        std::vector<VkImage> mDestroyedImages;
        std::vector<VkFramebuffer> mDestroyedFrameBuffers;
        std::vector<VkSemaphore> mDestroyeSemaphores;

        // fences
        std::vector<VkFence> mRecyleFences;
        std::vector<VkFence> mWaitFences;

        // semphore
        std::vector<VkSemaphore> mRecycledSemaphroes;

        // submissions
        std::vector<CommandListPtr> mSubmissions[QUEUE_INDEX_COUNT];

        void Begin(VkDevice device);
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
    Util::ObjectPool<Semaphore> mSemaphorePool;

    // vulkan object managers
    FenceManager mFencePoolManager;
    SemaphoreManager mSemaphoreManager;

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
    void SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire);

    SemaphorePtr RequestSemaphore();

    void ReleaseFrameBuffer(VkFramebuffer buffer);
    void ReleaseImage(VkImage image);
    void ReleaseImageView(VkImageView imageView);
    void ReleaseFence(VkFence fence, bool isWait);
    void ReleaseSemaphore(VkSemaphore semaphore, bool isSignalled);

    uint64_t GenerateCookie();

    // 必须在EndFrameContext之后调用,ReleaseSemaphroe仅在EndFrameContext中获取
    SemaphorePtr GetAndConsumeReleaseSemaphore();
    VkQueue GetPresentQueue()const;

private:

    // initial methods
    bool CheckPhysicalSuitable(const VkPhysicalDevice& device, bool isBreak);
    bool CheckExtensionSupport(const char* checkExtension, const std::vector<VkExtensionProperties>& availableExtensions);
    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice);
    std::vector<const char*> GetRequiredExtensions();

    // submit methods
    struct InternalFence
    {
        VkFence mFence = VK_NULL_HANDLE;
    };
    void SubmitQueue(QueueIndices queueIndex, InternalFence* fence = nullptr);
    void SubmitEmpty(QueueIndices queueIndex, InternalFence* fence);
    VkResult SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence);

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

    // internal wsi
    struct InternalWSI
    {
        SemaphorePtr mAcquire;
        SemaphorePtr mRelease;
        uint32_t mIndex = 0;
        VkQueue mPresentQueue;
        bool mConsumed = false;
    };
    InternalWSI mWSI;
};
