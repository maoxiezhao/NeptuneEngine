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

struct InitialImageBuffer
{
    BufferPtr buffer;
    std::array<VkBufferImageCopy, 32> bit;
    U32 numBit = 0;
};

struct WaitSemaphores
{
    std::vector<VkSemaphore> binaryWaits;
    std::vector<VkPipelineStageFlags> binaryWaitStages;
};

struct BatchComposer
{
public:
    BatchComposer();

    static const uint32_t MAX_SUBMIT_COUNT = 8;
    uint32_t submitIndex = 0;
    struct SubmitInfo
    {
        std::vector<VkPipelineStageFlags> waitStages;
        std::vector<VkSemaphore> waitSemaphores;
        std::vector<VkSemaphore> signalSemaphores;
        std::vector<VkCommandBuffer> commandLists;
    };
    SubmitInfo submitInfos[MAX_SUBMIT_COUNT];
    std::vector<VkSubmitInfo> submits;

private:
    void BeginBatch();

public:
    void AddWaitSemaphores(WaitSemaphores& semaphores);
    void AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages);
    void AddSignalSemaphore(VkSemaphore sem);
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
        {}
    };

    DeviceVulkan& device;
    Util::TempHashMap<FrameBufferNode, 8, false> framebuffers;
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

    // vulkan object pool, it is deleted last.
    Util::ObjectPool<CommandList> commandListPool;
    Util::ObjectPool<Image> imagePool;
    Util::ObjectPool<ImageView> imageViewPool;
    Util::ObjectPool<Fence> fencePool;
    Util::ObjectPool<Semaphore> semaphorePool;
    Util::ObjectPool<Sampler> samplers;
    Util::ObjectPool<Buffer> buffers;
    Util::ObjectPool<BufferView> bufferViews;
    Util::ObjectPool<ImageView> imageViews;
    Util::ObjectPool<Event> eventPool;

    // per frame resource
    struct FrameResource
    {
        DeviceVulkan& device;
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

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
        std::vector<VkEvent> destroyedEvents;

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

        FrameResource(DeviceVulkan& device_);
        ~FrameResource();

        void Begin();
    };
    std::vector<std::unique_ptr<FrameResource>> frameResources;
    uint32_t frameIndex = 0;

    FrameResource& CurrentFrameResource()
    {
        assert(frameIndex < frameResources.size());
        return *frameResources[frameIndex];
    }

    void InitFrameContext();

    // buffer pools
    BufferPool vboPool;
    BufferPool iboPool;

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
    void InitSwapchain(std::vector<VkImage>& images, VkFormat format, uint32_t width, uint32_t height);
    void BakeShaderProgram(ShaderProgram& program);
    void WaitIdle();
    bool IsSwapchainTouched();

    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandList(int threadIndex, QueueType queueType);
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isCompatible = false);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);
    PipelineLayout& RequestPipelineLayout(const CombinedResourceLayout& resLayout);
    SemaphorePtr RequestSemaphore();
    EventPtr RequestEvent();
    Shader& RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout = nullptr);
    ShaderProgram* RequestProgram(Shader* shaders[static_cast<U32>(ShaderStage::Count)]);
    DescriptorSetAllocator& RequestDescriptorSetAllocator(const DescriptorSetLayout& layout, const U32* stageForBinds);
    DescriptorSetAllocator* GetBindlessDescriptorSetAllocator(BindlessReosurceType type);
    BindlessDescriptorPoolPtr GetBindlessDescriptorPool(BindlessReosurceType type, U32 numSets, U32 numDescriptors);
    ImagePtr RequestTransientAttachment(U32 w, U32 h, VkFormat format, U32 index = 0, U32 samples = 1, U32 layers = 1);
    SamplerPtr RequestSampler(const SamplerCreateInfo& createInfo, bool isImmutable = false);
    ImmutableSampler* RequestImmutableSampler(const SamplerCreateInfo& createInfo);
    
    void RequestVertexBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestIndexBufferBlock(BufferBlock& block, VkDeviceSize size);
    void RequestBufferBlock(BufferBlock& block, VkDeviceSize size, BufferPool& pool, std::vector<BufferBlock>& recycle);

    ImagePtr CreateImage(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData);
    InitialImageBuffer CreateImageStagingBuffer(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData);
    ImagePtr CreateImageFromStagingBuffer(const ImageCreateInfo& createInfo, const InitialImageBuffer* stagingBuffer);
    ImageViewPtr CreateImageView(const ImageViewCreateInfo& viewInfo);
    BufferPtr CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData);
    BufferViewPtr CreateBufferView(const BufferViewCreateInfo& viewInfo);
    DeviceAllocationOwnerPtr AllocateMemmory(const MemoryAllocateInfo& allocInfo);

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
    void RecycleSemaphore(VkSemaphore semaphore);
    void ReleaseEvent(VkEvent ent);
    void FreeMemory(const DeviceAllocation& allocation);

    void* MapBuffer(const Buffer& buffer, MemoryAccessFlags flags);
    void UnmapBuffer(const Buffer& buffer, MemoryAccessFlags flags);

    void NextFrameContext();
    void EndFrameContext();
    void FlushFrame(QueueIndices queueIndex);
    void Submit(CommandListPtr& cmd, FencePtr* fence = nullptr, U32 semaphoreCount = 0, SemaphorePtr* semaphore = nullptr);
    void SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire);
    void AddWaitSemaphore(QueueType queueType, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush);
    void SetName(const Image& image, const char* name);
    void SetName(const Buffer& buffer, const char* name);

    bool IsImageFormatSupported(VkFormat format, VkFormatFeatureFlags required, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);

    ImmutableSampler* GetStockSampler(StockSampler type);
    uint64_t GenerateCookie();
    ShaderManager& GetShaderManager();
    SemaphorePtr GetAndConsumeReleaseSemaphore();
    VkQueue GetPresentQueue()const;
    DeviceFeatures GetFeatures()const { return features; }

    ImageView& GetSwapchainView();
    RenderPassInfo GetSwapchianRenderPassInfo(SwapchainRenderPassType swapchainRenderPassType);
    
private:
#ifdef VULKAN_TEST_FILESYSTEM
    void InitShaderManagerCache();
#endif

#ifdef VULKAN_TEST_FOSSILIZE
    
#endif

private:
    friend class CommandList;

    // stock samplers
    void InitStockSamplers();
    void InitStockSampler(StockSampler type);
    ImmutableSampler* stockSamplers[(int)StockSampler::Count] = {};

    // bindless
    void InitBindless();

    DescriptorSetAllocator* bindlessSampledImages = nullptr;
    DescriptorSetAllocator* bindlessStorageBuffers = nullptr;
    DescriptorSetAllocator* bindlessStorageImages = nullptr;
    DescriptorSetAllocator* bindlessSamplers = nullptr;

    // queue data
    struct QueueData
    {
        std::vector<SemaphorePtr> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitStages;
        bool needFence = false;
    };
    QueueData queueDatas[QUEUE_INDEX_COUNT];

    // submit methods
    struct InternalFence
    {
        VkFence fence = VK_NULL_HANDLE;
    };
    void SubmitImpl(CommandListPtr& cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore);
    void SubmitQueue(QueueIndices queueIndex, InternalFence* fence = nullptr, U32 semaphoreCount = 0, SemaphorePtr* semaphores = nullptr);
    void SubmitEmpty(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores);
    VkResult SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence);
    void SubmitStaging(CommandListPtr& cmd, VkBufferUsageFlags usage, bool flush);

    // internal wsi
    struct InternalWSI
    {
        SemaphorePtr acquire;
        SemaphorePtr release;
        uint32_t index = 0;
        VkQueue presentQueue = VK_NULL_HANDLE;
        bool consumed = false;
        std::vector<ImagePtr> swapchainImages;

        void Clear()
        {
            acquire.reset();
            release.reset();
            swapchainImages.clear();
        }
    };
    InternalWSI wsi;

    // shaders
    ShaderManager shaderManager;
};
}
}