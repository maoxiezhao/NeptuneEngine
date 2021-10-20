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
#include "descriptorSet.h"

#include <set>
#include <unordered_map>

namespace GPU
{

struct Swapchain
{
    VkDevice& device;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR format = {};
    VkExtent2D swapchainExtent = {};
    std::vector<ImagePtr> images;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    uint32_t mImageIndex = 0;

    Swapchain(VkDevice& device_) : device(device_)
    {
    }

    ~Swapchain()
    {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
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
    class ImageNode : public Util::TempHashMapItem<ImagePtr>
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

class DeviceVulkan
{
public:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkInstance instance = VK_NULL_HANDLE;
    QueueInfo queueInfo;
    DeviceFeatures features;

    // per frame resource
    struct FrameResource
    {
        std::vector<CommandPool> cmdPools[QueueIndices::QUEUE_INDEX_COUNT];

        // destroyed resoruces
        std::vector<VkImageView> destroyedImageViews;
        std::vector<VkImage> destroyedImages;
        std::vector<VkFramebuffer> destroyedFrameBuffers;
        std::vector<VkSemaphore> destroyeSemaphores;
        std::vector<VkPipeline> destroyedPipelines;

        // fences
        std::vector<VkFence> recyleFences;
        std::vector<VkFence> waitFences;

        // semphore
        std::vector<VkSemaphore> recycledSemaphroes;

        // submissions
        std::vector<CommandListPtr> submissions[QUEUE_INDEX_COUNT];

        void Begin(DeviceVulkan& device);
        void ProcessDestroyed(DeviceVulkan& device);
    };
    std::vector<FrameResource> frameResources;
    uint32_t frameIndex = 0;

    FrameResource& CurrentFrameResource()
    {
        assert(frameIndex < frameResources.size());
        return frameResources[frameIndex];
    }

    void InitFrameContext();

    // vulkan object cache
    // TODO: use vk_mem_alloc to replace vulkanCache
    VulkanCache<Shader> shaders;
    VulkanCache<ShaderProgram> programs;
    VulkanCache<RenderPass> renderPasses;
    VulkanCache<FrameBuffer> frameBuffers;
    VulkanCache<PipelineLayout> pipelineLayouts;
    VulkanCache<DescriptorSetAllocator> descriptorSetAllocators;

    // vulkan object pool (release perframe)
    Util::ObjectPool<CommandList> commandListPool;
    Util::ObjectPool<Image> imagePool;
    Util::ObjectPool<ImageView> imageViewPool;
    Util::ObjectPool<Fence> fencePool;
    Util::ObjectPool<Semaphore> semaphorePool;

    // vulkan object managers
    FenceManager fencePoolManager;
    SemaphoreManager semaphoreManager;
    TransientAttachmentAllcoator transientAllocator;
    FrameBufferAllocator frameBufferAllocator;

public:
    DeviceVulkan();
    ~DeviceVulkan();

    void operator=(DeviceVulkan&&) = delete;
    DeviceVulkan(DeviceVulkan&&) = delete;

    void SetContext(VulkanContext& context);
    bool CreateSwapchain(Swapchain*& swapchain, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void BakeShaderProgram(ShaderProgram& program);
    void WaitIdle();
    bool IsSwapchainTouched();

    CommandListPtr RequestCommandList(QueueType queueType);
    CommandListPtr RequestCommandList(int threadIndex, QueueType queueType);
    RenderPass& RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isCompatible = false);
    FrameBuffer& RequestFrameBuffer(const RenderPassInfo& renderPassInfo);
    PipelineLayout& RequestPipelineLayout(const CombinedResourceLayout& resLayout);
    SemaphorePtr RequestSemaphore();
    Shader& RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout = nullptr);
    ShaderProgram* RequestProgram(Shader* shaders[static_cast<U32>(ShaderStage::Count)]);
    DescriptorSetAllocator& RequestDescriptorSetAllocator(const DescriptorSetLayout& layout, const U32* stageForBinds);
    ImagePtr RequestTransientAttachment(U32 w, U32 h, VkFormat format, U32 index = 0, U32 samples = 1, U32 layers = 1);

    ImagePtr CreateImage(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData);

    void BeginFrameContext();
    void EndFrameContext();
    void Submit(CommandListPtr& cmd);
    void SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire);
    void SetName(const Image& image, const char* name);

    void ReleaseFrameBuffer(VkFramebuffer buffer);
    void ReleaseImage(VkImage image);
    void ReleaseImageView(VkImageView imageView);
    void ReleaseFence(VkFence fence, bool isWait);
    void ReleaseSemaphore(VkSemaphore semaphore, bool isSignalled);
    void ReleasePipeline(VkPipeline pipeline);

    uint64_t GenerateCookie();
    ShaderManager& GetShaderManager();
    SemaphorePtr GetAndConsumeReleaseSemaphore();
    VkQueue GetPresentQueue()const;

    RenderPassInfo GetSwapchianRenderPassInfo(const Swapchain& swapChain, SwapchainRenderPassType swapchainRenderPassType);
    bool ReflectShader(ShaderResourceLayout& layout, const U32 *spirvData, size_t spirvSize);

private:
    friend class CommandList;

    // submit methods
    struct InternalFence
    {
        VkFence fence = VK_NULL_HANDLE;
    };
    void SubmitQueue(QueueIndices queueIndex, InternalFence* fence = nullptr);
    void SubmitEmpty(QueueIndices queueIndex, InternalFence* fence);
    VkResult SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence);

    // internal wsi
    struct InternalWSI
    {
        SemaphorePtr acquire;
        SemaphorePtr release;
        uint32_t index = 0;
        VkQueue presentQueue = VK_NULL_HANDLE;
        bool consumed = false;
    };
    InternalWSI wsi;

    // shaders
    ShaderManager shaderManager;
};


}