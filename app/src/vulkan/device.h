#pragma once

#include "definition.h"
#include "common.h"
#include "context.h"
#include "commandList.h"
#include "image.h"
#include "renderPass.h"
#include "fence.h"
#include "semaphore.h"
#include "shader.h"
#include "shaderManager.h"

#include <set>
#include <unordered_map>

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
    VkDevice mDevice;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkInstance mInstance;
    QueueInfo mQueueInfo;

    // per frame resource
    struct FrameResource
    {
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

        // destroyed resoruces
        std::vector<VkImageView> mDestroyedImageViews;
        std::vector<VkImage> mDestroyedImages;
        std::vector<VkFramebuffer> mDestroyedFrameBuffers;
        std::vector<VkSemaphore> mDestroyeSemaphores;
        std::vector<VkPipeline> mDestroyedPipelines;

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
    VulkanCache<Shader> mShaders;
    VulkanCache<RenderPass> mRenderPasses;
    VulkanCache<FrameBuffer> mFrameBuffers;
    VulkanCache<PipelineLayout> mPipelineLayouts;

    Util::ObjectPool<CommandList> mCommandListPool;
    Util::ObjectPool<Image> mImagePool;
    Util::ObjectPool<ImageView> mImageViewPool;
    Util::ObjectPool<Fence> mFencePool;
    Util::ObjectPool<Semaphore> mSemaphorePool;

    // vulkan object managers
    FenceManager mFencePoolManager;
    SemaphoreManager mSemaphoreManager;

public:
    DeviceVulkan();
    ~DeviceVulkan();

    void operator=(DeviceVulkan&&) = delete;
    DeviceVulkan(DeviceVulkan&&) = delete;

    void SetContext(VulkanContext& context);
    bool CreateSwapchain(Swapchain*& swapchain, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void BakeShaderProgram(ShaderProgram& program);

    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandList(int threadIndex, QueueType queueType);
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isComatible = false);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);
    PipelineLayout& RequestPipelineLayout();
    SemaphorePtr RequestSemaphore();
    Shader& RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength);

    void BeginFrameContext();
    void EndFrameContext();
    void Submit(CommandListPtr& cmd);
    void SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire);

    void ReleaseFrameBuffer(VkFramebuffer buffer);
    void ReleaseImage(VkImage image);
    void ReleaseImageView(VkImageView imageView);
    void ReleaseFence(VkFence fence, bool isWait);
    void ReleaseSemaphore(VkSemaphore semaphore, bool isSignalled);
    void ReleasePipeline(VkPipeline pipeline);

    uint64_t GenerateCookie();
    ShaderManager& GetShaderManager();

    // 必须在EndFrameContext之后调用,ReleaseSemaphroe仅在EndFrameContext中获取
    SemaphorePtr GetAndConsumeReleaseSemaphore();
    VkQueue GetPresentQueue()const;

    RenderPassInfo GetSwapchianRenderPassInfo(const Swapchain& swapChain, SwapchainRenderPassType swapchainRenderPassType);

private:
    friend class CommandList;

    void UpdateGraphicsPipelineHash(CompilePipelineState pipeline);

    // submit methods
    struct InternalFence
    {
        VkFence mFence = VK_NULL_HANDLE;
    };
    void SubmitQueue(QueueIndices queueIndex, InternalFence* fence = nullptr);
    void SubmitEmpty(QueueIndices queueIndex, InternalFence* fence);
    VkResult SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence);

    // internal wsi
    struct InternalWSI
    {
        SemaphorePtr mAcquire;
        SemaphorePtr mRelease;
        uint32_t mIndex = 0;
        VkQueue mPresentQueue = VK_NULL_HANDLE;
        bool mConsumed = false;
    };
    InternalWSI mWSI;

    // shaders
    ShaderManager mShaderManager;
};
