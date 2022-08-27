#pragma once

#include "definition.h"
#include "context.h"
#include "memory.h"
#include "commandList.h"
#include "image.h"
#include "buffer.h"
#include "bufferPool.h"
#include "renderPass.h"
#include "fence.h"
#include "semaphore.h"
#include "shader.h"
#include "shaderManager.h"
#include "descriptorSet.h"
#include "TextureFormatLayout.h"
#include "sampler.h"
#include "event.h"

#include "core\platform\sync.h"
#include "core\utils\threadLocal.h"

#include <array>
#include <set>
#include <unordered_map>

namespace VulkanTest
{
namespace GPU
{

enum class SwapchainRenderPassType
{
    ColorOnly,
    Depth,
    DepthStencil
};

struct SwapChainDesc
{
    U32 width = 0;
    U32 height = 0;
    U32 bufferCount = 2;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    bool fullscreen = false;
    bool vsync = true;
    float clearColor[4] = { 0,0,0,1 };
};

enum class SwapchainError
{
    None,
    NoSurface,
    Error
};

struct SwapChain
{
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    U32 swapchainWidth = 0;
    U32 swapchainHeight = 0;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    U32 swapchainImageIndex = 0;
    std::vector<VkImage> vkImages;
    std::vector<ImagePtr> images;
    std::vector<GPU::SemaphorePtr> releaseSemaphores;
};

struct InitialImageBuffer
{
    BufferPtr buffer;
    std::array<VkBufferImageCopy, 32> blits;
    U32 numBlits = 0;
};

struct WaitSemaphores
{
    std::vector<VkSemaphore> binaryWaits;
    std::vector<VkPipelineStageFlags> binaryWaitStages;
    std::vector<VkSemaphore> timelineWaits;
    std::vector<VkPipelineStageFlags> timelineWaitStages;
    std::vector<U64> timelineWaitCounts;
};

struct BatchComposer
{
public:
    BatchComposer();

    static const uint32_t MAX_SUBMISSIONS = 8;
    uint32_t submitIndex = 0;
    struct SubmitInfo
    {
        std::vector<VkPipelineStageFlags> waitStages;
        std::vector<uint64_t> waitCounts;
        std::vector<VkSemaphore> waitSemaphores;
        std::vector<uint64_t> signalCounts;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<VkCommandBuffer> commandLists;
    };
    SubmitInfo submitInfos[MAX_SUBMISSIONS];
    std::vector<VkSubmitInfo> submits;
    VkTimelineSemaphoreSubmitInfo timelineInfos[MAX_SUBMISSIONS];

private:
    void BeginBatch();

    bool hasTimelineSemaphoreInBatch(U32 index)const;

public:
    void AddWaitSemaphores(WaitSemaphores& semaphores);
    void AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages);
    void AddSignalSemaphore(VkSemaphore sem, U64 timeline);
    void AddCommandBuffer(VkCommandBuffer buffer);
    std::vector<VkSubmitInfo>& Bake();
};

class TransientAttachmentAllcoator
{
public:
    TransientAttachmentAllcoator(DeviceVulkan& device_) : device(device_) {}

    void BeginFrame();
    void Clear();
    ImagePtr RequsetAttachment(U32 w, U32 h, VkFormat format, U32 index = 0, U32 samples = 1, U32 layers = 1);

private:
    class ImageNode : public Util::TempHashMapItem<ImageNode>
    {
    public:
        explicit ImageNode(ImagePtr image_) : image(image_) {}
        ImagePtr image;
    };

    DeviceVulkan& device;
    Util::TempHashMap<ImageNode, 8, false> attachments;

#ifdef VULKAN_MT
    // Mutex mutex;
    std::mutex mutex;
#endif
};

class FrameBufferAllocator
{
public:
    FrameBufferAllocator(DeviceVulkan& device_) : device(device_) {}

    void BeginFrame();
    void Clear();
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& info);

private:
    class FrameBufferNode : public Util::TempHashMapItem<FrameBufferNode>, public FrameBuffer
    {
    public:
        explicit FrameBufferNode(DeviceVulkan& device_, RenderPass& renderPass_, const RenderPassInfo& info_) :
            FrameBuffer(device_, renderPass_, info_)
        {
            SetInternalSyncObject();
        }
    };

    DeviceVulkan& device;
    Util::TempHashMap<FrameBufferNode, 8, false> framebuffers;

#ifdef VULKAN_MT
    // Mutex mutex;
    std::mutex mutex;
#endif
};

#ifdef VULKAN_TEST_FOSSILIZE
class DeviceVulkan
#else
class DeviceVulkan
#endif
{
public:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    QueueInfo queueInfo;
    DeviceFeatures features;
    SystemHandles systemHandles;

#ifdef VULKAN_MT
    std::mutex mutex;
    std::condition_variable cond;
    U32 frameCounter = 0;
#endif

    // vulkan object pool, it is deleted last.
    ObjectPool<CommandList> commandListPool;
    ObjectPool<Image> imagePool;
    ObjectPool<ImageView> imageViewPool;
    ObjectPool<Fence> fencePool;
    ObjectPool<Semaphore> semaphorePool;
    ObjectPool<Sampler> samplers;
    ObjectPool<Buffer> buffers;
    ObjectPool<BufferView> bufferViews;
    ObjectPool<ImageView> imageViews;
    ObjectPool<Event> eventPool;
    ObjectPool<BindlessDescriptorHandler> bindlessDescriptorHandlers;

    // per frame resource
    struct FrameResource
    {
        FrameResource(DeviceVulkan& device_, U32 frameIndex_);
        ~FrameResource();

        void operator=(const FrameResource&) = delete;
        FrameResource(const FrameResource&) = delete;

        void Begin();
        void TrimCommandPools();

        DeviceVulkan& device;
        U32 frameIndex;    
        ThreadLocalObject<CommandPool, 32> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

        // timeline
        VkSemaphore timelineSemaphores[QUEUE_INDEX_COUNT] = {};
        uint64_t timelineValues[QUEUE_INDEX_COUNT] = {};

        // destroyed resoruces
        std::vector<VkImageView> destroyedImageViews;
        std::vector<VkImage> destroyedImages;
        std::vector<VkFramebuffer> destroyedFrameBuffers;
        std::vector<VkSemaphore> destroyeSemaphores;
        std::vector<VkPipeline> destroyedPipelines;
        std::vector<VkBuffer> destroyedBuffers;
        std::vector<VkBufferView> destroyedBufferViews;
        std::vector<VkDescriptorPool> destroyedDescriptorPool;
        std::vector<VkSampler> destroyedSamplers;
        std::vector<VkEvent> recyledEvents;
        std::vector<VkShaderModule> destroyedShaders;

        // bindless
        std::vector<std::pair<I32, BindlessReosurceType>> destroyedBindlessResources;

        // memory
        std::vector<DeviceAllocation> destroyedAllocations;

        // fences
        std::vector<VkFence> recyleFences;
        std::vector<VkFence> waitFences;

        // semphore
        std::vector<VkSemaphore> recycledSemaphroes;

        // submissions
        std::vector<CommandListPtr> submissions[QUEUE_INDEX_COUNT];

        // buffer blocks
        std::vector<BufferBlock> vboBlocks;
        std::vector<BufferBlock> iboBlocks;
        std::vector<BufferBlock> uboBlocks;
        std::vector<BufferBlock> stagingBlocks;
        std::vector<BufferBlock> storageBlocks;

        // persistent buffer blocks
        std::unordered_map<VkCommandBuffer, BufferBlock> storageBlockMap;
    };
    std::vector<std::unique_ptr<FrameResource>> frameResources;
    uint32_t frameIndex = 0;

    constexpr U32 GetFrameIndex()
    {
        return frameIndex;
    }

    FrameResource& CurrentFrameResource()
    {
        assert(frameIndex < frameResources.size());
        return *frameResources[frameIndex];
    }

    void InitFrameContext(U32 count);

    // buffer pools
    BufferPool vboPool;
    BufferPool iboPool;
    BufferPool uboPool;
    BufferPool stagingPool;
    BufferPool storagePool;

    // vulkan object cache
    VulkanCache<Shader> shaders;
    VulkanCache<ShaderProgram> programs;
    VulkanCache<RenderPass> renderPasses;
    VulkanCache<PipelineLayout> pipelineLayouts;
    VulkanCache<DescriptorSetAllocator> descriptorSetAllocators;
    VulkanCache<DeviceAllocationOwner> allocations;
    VulkanCache<BindlessDescriptorPool> bindlessDescriptorPools;
    VulkanCache<ImmutableSampler> immutableSamplers;

    // vulkan object managers
    FenceManager fencePoolManager;
    SemaphoreManager semaphoreManager;
    EventManager eventManager;
    TransientAttachmentAllcoator transientAllocator;
    FrameBufferAllocator frameBufferAllocator;
    DeviceAllocator memory;

public:
    DeviceVulkan();
    ~DeviceVulkan();

    void operator=(DeviceVulkan&&) = delete;
    DeviceVulkan(DeviceVulkan&&) = delete;

    void SetContext(VulkanContext& context);
    SwapchainError CreateSwapchain(const SwapChainDesc&desc, VkSurfaceKHR surface, SwapChain* swapchain);
    void WaitIdle();
    void WaitIdleNolock();
    bool IsSwapchainTouched();
    void MoveReadWriteCachesToReadOnly();

    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandListForThread(QueueType queueType);
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isCompatible = false);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);
    PipelineLayout* RequestPipelineLayout(const CombinedResourceLayout& resLayout);
    SemaphorePtr RequestSemaphore();
    SemaphorePtr RequestEmptySemaphore();
    EventPtr RequestEvent();
    EventPtr RequestSignalEvent(VkPipelineStageFlags stages);
    Shader* RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout = nullptr);
    Shader* RequestShaderByHash(HashValue hash);
    ShaderProgram* RequestProgram(const Shader* shaders[static_cast<U32>(ShaderStage::Count)]);
    DescriptorSetAllocator& RequestDescriptorSetAllocator(const DescriptorSetLayout& layout, const U32* stageForBinds);
    BindlessDescriptorPoolPtr GetBindlessDescriptorPool(BindlessReosurceType type, U32 numSets, U32 numDescriptors);
    ImagePtr RequestTransientAttachment(U32 w, U32 h, VkFormat format, U32 index = 0, U32 samples = 1, U32 layers = 1);
    SamplerPtr RequestSampler(const SamplerCreateInfo& createInfo, bool isImmutable = false);
    ImmutableSampler* RequestImmutableSampler(const SamplerCreateInfo& createInfo);
    
    void RequestVertexBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestVertexBufferBlockNolock(BufferBlock& block, VkDeviceSize size);
    void RequestIndexBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestIndexBufferBlockNoLock(BufferBlock& block, VkDeviceSize size);
    void RequestUniformBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestUniformBufferBlockNoLock(BufferBlock& block, VkDeviceSize size);
    void RequestStagingBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestStagingBufferBlockNolock(BufferBlock& block, VkDeviceSize size);
    void RequestStorageBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestStorageBufferBlockNolock(BufferBlock& block, VkDeviceSize size);
    void RecordStorageBufferBlock(BufferBlock& block, CommandList& cmd);

    ImagePtr CreateImage(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData);
    InitialImageBuffer CreateImageStagingBuffer(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData);
    ImagePtr CreateImageFromStagingBuffer(const ImageCreateInfo& createInfo, const InitialImageBuffer* stagingBuffer);
    ImageViewPtr CreateImageView(const ImageViewCreateInfo& viewInfo);
    BufferPtr CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData);
    BufferViewPtr CreateBufferView(const BufferViewCreateInfo& viewInfo);
    DeviceAllocationOwnerPtr AllocateMemmory(const MemoryAllocateInfo& allocInfo);

    BindlessDescriptorPtr CreateBindlessStroageBuffer(const Buffer& buffer, VkDeviceSize offset, VkDeviceSize range);
    BindlessDescriptorPtr CreateBindlessSampledImage(const ImageView& imageView, VkImageLayout imageLayout);

    void ReleaseFrameBuffer(VkFramebuffer buffer);
    void ReleaseImage(VkImage image);
    void ReleaseImageView(VkImageView imageView);
    void ReleaseFence(VkFence fence, bool isWait);
    void ReleasePipeline(VkPipeline pipeline);
    void ReleaseBuffer(VkBuffer buffer);
    void ReleaseBufferView(VkBufferView bufferView);
    void ReleaseSampler(VkSampler sampler);
    void ReleaseDescriptorPool(VkDescriptorPool pool);
    void ReleaseSemaphore(VkSemaphore semaphore);
    void ReleaseShader(Shader* shader);
    void RecycleSemaphore(VkSemaphore semaphore);
    void ReleaseEvent(VkEvent ent);
    void FreeMemory(const DeviceAllocation& allocation);
    void ReleaseBindlessResource(I32 index, BindlessReosurceType type);

    void ReleaseFrameBufferNolock(VkFramebuffer buffer);
    void ReleaseImageNolock(VkImage image);
    void ReleaseImageViewNolock(VkImageView imageView);
    void ReleaseFenceNolock(VkFence fence, bool isWait);
    void ReleasePipelineNolock(VkPipeline pipeline);
    void ReleaseBufferNolock(VkBuffer buffer);
    void ReleaseBufferViewNolock(VkBufferView bufferView);
    void ReleaseSamplerNolock(VkSampler sampler);
    void ReleaseDescriptorPoolNolock(VkDescriptorPool pool);
    void ReleaseShaderNolock(Shader* shader);
    void ReleaseSemaphoreNolock(VkSemaphore semaphore);
    void RecycleSemaphoreNolock(VkSemaphore semaphore);
    void ReleaseEventNolock(VkEvent ent);
    void FreeMemoryNolock(const DeviceAllocation& allocation);
    void ReleaseBindlessResourceNoLock(I32 index, BindlessReosurceType type);

    void* MapBuffer(const Buffer& buffer, MemoryAccessFlags flags);
    void UnmapBuffer(const Buffer& buffer, MemoryAccessFlags flags);

    void NextFrameContext();
    void EndFrameContext();
    void EndFrameContextNolock();
    void FlushFrames();
    void FlushFrame(QueueIndices queueIndex);
    void Submit(CommandListPtr& cmd, FencePtr* fence = nullptr, U32 semaphoreCount = 0, SemaphorePtr* semaphore = nullptr);
    void SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire);
    void AddWaitSemaphore(QueueType queueType, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush);
    void AddWaitSemaphore(QueueIndices queueIndex, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush);
    void AddWaitSemaphoreNolock(QueueIndices queueType, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush);
    void SetName(const Image& image, const char* name);
    void SetName(const Buffer& buffer, const char* name);
    void SetName(const Sampler& sampler, const char* name);

    bool IsImageFormatSupported(VkFormat format, VkFormatFeatureFlags required, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);
    U64 GetMinOffsetAlignment() const;

    ImmutableSampler* GetStockSampler(StockSampler type);
    uint64_t GenerateCookie();
    ShaderManager& GetShaderManager();
    SemaphorePtr GetAndConsumeReleaseSemaphore();
    VkQueue GetPresentQueue()const;
    DeviceFeatures GetFeatures()const { return features; }
    SystemHandles& GetSystemHandles() { return systemHandles; }

    RenderPassInfo GetSwapchianRenderPassInfo(const SwapChain* swapchain, SwapchainRenderPassType swapchainRenderPassType = SwapchainRenderPassType::DepthStencil);  
    CommandListPtr RequestCommandListNolock(QueueType queueType);

    QueueIndices GetPhysicalQueueType(QueueType type)const;
    VkFormat GetDefaultDepthStencilFormat() const;
    VkFormat GetDefaultDepthFormat() const;
    constexpr U64 GetFrameCount() const { return FRAMECOUNT; }
    VkInstance GetInstance() { return instance; }
    bool IsRendering()const { return isRendering; }

    BindlessDescriptorHeap* GetBindlessDescriptorHeap(BindlessReosurceType type);

    void InitPipelineCache();
    bool InitPipelineCache(const U8* data, size_t size);
    void FlushPipelineCache();
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    void InitShaderManagerCache();

    static bool InitRenderdocCapture();

    static constexpr U32 IMMUTABLE_SAMPLER_SLOT_BEGIN = 100;

private:
    friend class CommandList;

    U64 FRAMECOUNT = 0;
    bool isRendering = false;

    void AddFrameCounter();
    void DecrementFrameCounter();

    // stock samplers
    void InitStockSamplers();
    void InitStockSampler(StockSampler type);
    ImmutableSampler* stockSamplers[(int)StockSampler::Count] = {};

    // bindless
    void InitBindless();
    DescriptorSetAllocator* GetBindlessDescriptorSetAllocator(BindlessReosurceType type);

    DescriptorSetAllocator* bindlessSampledImagesSetAllocator = nullptr;
    DescriptorSetAllocator* bindlessStorageBuffersSetAllocator = nullptr;
    DescriptorSetAllocator* bindlessStorageImagesSetAllocator = nullptr;
    DescriptorSetAllocator* bindlessSamplersSetAllocator = nullptr;

    struct BindlessHandler
    {
        BindlessDescriptorHeap bindlessStorageBuffers;
        BindlessDescriptorHeap bindlessSampledImages;

        ~BindlessHandler()
        {
            bindlessStorageBuffers.Destroy();
            bindlessSampledImages.Destroy();
        }
    }
    bindlessHandler;

    // copy from CPU to GPU before submitting graphics or compute work.
    struct
    {
        std::vector<BufferBlock> vbo;
        std::vector<BufferBlock> ibo;
        std::vector<BufferBlock> ubo;
    }
    pendingBufferBlocks;
    void RequestBufferBlock(BufferBlock& block, VkDeviceSize size, BufferPool& pool, std::vector<BufferBlock>& recycle, std::vector<BufferBlock>* pending);
    void SyncPendingBufferBlocks();

    // queue data
    struct QueueData
    {
        std::vector<SemaphorePtr> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitStages;
        VkSemaphore timelineSemaphore = VK_NULL_HANDLE;
        U64 timeline = 0;
        bool needFence = false;
    };
    QueueData queueDatas[QUEUE_INDEX_COUNT];
    std::vector<U32> queueFamilies;

    void InitTimelineSemaphores();
    void DeinitTimelineSemaphores();

    // submit methods
    struct InternalFence
    {
        VkSemaphore timeline;
        U64 value;
    };
    void SubmitNolock(CommandListPtr cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore);
    void SubmitQueue(QueueIndices queueIndex, InternalFence* fence = nullptr, U32 semaphoreCount = 0, SemaphorePtr* semaphores = nullptr);
    void SubmitEmpty(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores);
    VkResult SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence);
    void SubmitStaging(CommandListPtr& cmd, VkBufferUsageFlags usage, bool flush);
    void LogDeviceLost();
    void CollectWaitSemaphores(QueueData& data, WaitSemaphores& waitSemaphores);
    void EmitQueueSignals(BatchComposer& composer, VkSemaphore sem, U64 timeline, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores);

    // internal wsi
    struct InternalWSI
    {
        SemaphorePtr acquire;
        SemaphorePtr release;
        uint32_t index = 0;
        VkQueue presentQueue = VK_NULL_HANDLE;
        bool consumed = false;

        void Clear()
        {
            acquire.reset();
            release.reset();
        }
    };
    InternalWSI wsi;

    // shaders
    ShaderManager shaderManager;
};
}
}