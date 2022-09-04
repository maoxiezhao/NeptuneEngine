#pragma once

#include "gpu\vulkan\device.h"
#include "core\threading\jobsystem.h"
#include "core\utils\string.h"

#include <unordered_set>

namespace VulkanTest
{
class RenderGraph;

struct VULKAN_TEST_API RenderPassJob
{
    virtual~RenderPassJob() = default;

    virtual bool IsConditional()const;
    virtual bool GetClearDepthStencil(VkClearDepthStencilValue* value) const;
    virtual bool GetClearColor(unsigned attachment, VkClearColorValue* value) const;

    virtual void Setup(GPU::DeviceVulkan& device);
    virtual void SetupDependencies(RenderGraph& graph);
    virtual void EnqueuePrepare(RenderGraph& graph);
    virtual void BuildRenderPass(GPU::CommandList& cmd);
};

enum class RenderGraphQueueFlag
{
    Graphics = 1 << 0,
    Compute = 1 << 1,
    AsyncCompute = 1 << 2,
    AsyncGraphcs = 1 << 3,
};

enum class AttachmentSizeType
{
    Absolute,
    SwapchainRelative,
};

enum class RenderGraphResourceType
{
    Texture,
    Buffer,
    Proxy
};

struct AttachmentInfo
{
    F32 sizeX = 1.0f;
    F32 sizeY = 1.0f;
    F32 sizeZ = 1.0f;
    U32 layers = 1;
    U32 samples = 1;
    U32 levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    AttachmentSizeType sizeType = AttachmentSizeType::Absolute;
};

struct BufferInfo
{
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;

    bool operator==(const BufferInfo& other) const
    {
        return size == other.size &&
            usage == other.usage;
    }

    bool operator!=(const BufferInfo& other) const
    {
        return !(*this == other);
    }
};

struct ResourceDimensions
{
    VkFormat format = VK_FORMAT_UNDEFINED;
    U32 width = 0;
    U32 height = 0;
    U32 depth = 1;
    U32 layers = 1;
    U32 levels = 1;
    U32 samples = 1;
    U32 queues = 0;
    VkImageUsageFlags imageUsage = 0;
    bool isTransient = false;
    BufferInfo bufferInfo = {};
    String name;

    bool operator==(const ResourceDimensions& other) const
    {
        return format == other.format &&
            width == other.width &&
            height == other.height &&
            depth == other.depth &&
            layers == other.layers &&
            levels == other.levels &&
            bufferInfo == other.bufferInfo;
    }

    bool operator!=(const ResourceDimensions& other) const
    {
        return !(*this == other);
    }

    bool IsBuffer()const
    {
        return bufferInfo.size > 0;
    }

    bool IsStorageImage()const
    {
        return (imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    }

    bool IsBufferLikeRes()const
    {
        return (imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) != 0 || (bufferInfo.size > 0);
    }

    bool UseSemaphore()const
    {
        // Need to use emaphore when using muti queues
        U32 physicalQueue = queues;

        // Regular compute uses regular graphics queue.
        if (physicalQueue & (U32)RenderGraphQueueFlag::Compute)
            physicalQueue |= (U32)RenderGraphQueueFlag::Graphics;

        physicalQueue &= ~(U32)RenderGraphQueueFlag::Compute;
        return (physicalQueue & (physicalQueue - 1)) != 0;
    }
};

class VULKAN_TEST_API RenderResource
{
public:
    RenderResource(U32 index_, RenderGraphResourceType resType_) :  index(index_), resType(resType_) {}
    virtual ~RenderResource() {};

    const std::unordered_set<U32>& GetWrittenPasses()const
    {
        return writtenPasses;
    }

    const std::unordered_set<U32>& GetReadPasses()const
    {
        return readPasses;
    }

    std::unordered_set<U32>& GetWrittenPasses()
    {
        return writtenPasses;
    }

    std::unordered_set<U32>& GetReadPasses()
    {
        return readPasses;
    }

    void SetName(const char* name_)
    {
        name = name_;
    }

    const String& GetName()const
    {
        return name;
    }

    void PassRead(U32 passIndex)
    {
        readPasses.insert(passIndex);
    }

    void PassWrite(U32 passIndex)
    {
        writtenPasses.insert(passIndex);
    }

    enum { Unused = ~0u };

    void SetPhysicalIndex(unsigned index_)
    {
        physicalIndex = index_;
    }

    U32 GetPhysicalIndex() const
    {
        return physicalIndex;
    }

    void AddUsedQueue(U32 queue)
    {
        usedQueues |= queue;
    }

    U32 GetUsedQueues()const
    {
        return usedQueues;
    }

    RenderGraphResourceType GetResourceType()const
    {
        return resType;
    }

protected:
    friend class RenderGraph;

    U32 index;
    String name;
    RenderGraphResourceType resType;
    std::unordered_set<U32> writtenPasses;
    std::unordered_set<U32> readPasses;
    U32 physicalIndex = Unused;
    U32 usedQueues = 0;
};

class VULKAN_TEST_API RenderTextureResource : public RenderResource
{
public:
    RenderTextureResource(U32 index_) : RenderResource(index_, RenderGraphResourceType::Texture) {}

    void SetImageUsage(VkImageUsageFlags flags)
    {
        usage |= flags;
    }

    VkImageUsageFlags GetImageUsage()const
    {
        return usage;
    }

    void SetAttachmentInfo(const AttachmentInfo& info_)
    {
        info = info_;
    }

    const AttachmentInfo& GetAttachmentInfo()const
    {
        return info;
    }

private:
    AttachmentInfo info;
    VkImageUsageFlags usage = 0;
};

class VULKAN_TEST_API RenderBufferResource : public RenderResource
{
public:
    RenderBufferResource(U32 index_) : RenderResource(index_, RenderGraphResourceType::Buffer) {}

    void SetBufferUsage(VkBufferUsageFlags flags)
    {
        usage |= flags;
    }

private:
    VkBufferUsageFlags usage = 0;
};

using EnqueuePrepareFunc = std::function<void()>;
using BuildRenderPassFunc = std::function<void(GPU::CommandList&)>;
using ClearDepthStencilFunc = std::function<bool(VkClearDepthStencilValue* value)>;
using ClearColorFunc = std::function<bool(U32 index, VkClearColorValue* value)>;

class VULKAN_TEST_API RenderPass
{
public:
    enum { Unused = ~0u };

    struct AccessedResource
    {
        VkPipelineStageFlags stages = 0;
        VkAccessFlags access = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };
    struct AccessedTextureResource : AccessedResource
    {
        RenderTextureResource* texture = nullptr;
    };

    struct AccessedProxyResource : AccessedResource
    {
        RenderResource* proxy = nullptr;
    };

    RenderPass(RenderGraph& graph_, U32 index_, U32 queue_);
    ~RenderPass();

    void SetEnqueuePprepareCallback(EnqueuePrepareFunc func)
    {
        enqueuePrepareCallback = std::move(func);
    }

    void SetBuildCallback(BuildRenderPassFunc func)
    {
        buildRenderPassCallback = std::move(func);
    }

    void SetClearDepthStencilCallback(ClearDepthStencilFunc func)
    {
        clearDepthStencilCallback = std::move(func);
    }

    void SetClearColorCallback(ClearColorFunc func)
    {
        clearColorCallback = std::move(func);
    }

    bool GetClearColor(U32 index, VkClearColorValue* value = nullptr)
    {
        if (clearColorCallback != nullptr)
            return clearColorCallback(index, value);
        return false;
    }

    bool GetClearDepthStencil(VkClearDepthStencilValue* value = nullptr)
    {
        if (clearDepthStencilCallback != nullptr)
            return clearDepthStencilCallback(value);
        return false;
    }

    void EnqueuePrepareRenderPass()
    {
        if (enqueuePrepareCallback != nullptr)
            enqueuePrepareCallback();
    }

    void BuildRenderPass(GPU::CommandList& cmd)
    {
        if (buildRenderPassCallback != nullptr)
            return buildRenderPassCallback(cmd);
    }

    bool NeedRenderPass() const
    {
        return true;
    }
    
    RenderTextureResource& ReadTexture(const char* name, VkPipelineStageFlags stages = 0);
    RenderTextureResource& ReadDepthStencil(const char* name);
    RenderTextureResource& WriteColor(const char* name, const AttachmentInfo& info, const char* input = nullptr);
    RenderTextureResource& WriteDepthStencil(const char* name, const AttachmentInfo& info);
    RenderTextureResource& WriteStorageTexture(const char* name, const AttachmentInfo& info, const char* input = "");
    RenderBufferResource& ReadStorageBuffer(const char* name);
    RenderBufferResource& WriteStorageBuffer(const char* name, const BufferInfo& info, const char* input);

    void AddProxyOutput(const char* name, VkPipelineStageFlags stages);
    void AddProxyInput(const char* name, VkPipelineStageFlags stages);

    void AddFakeResourceWriteAlias(const char* from, const char* to);

    const RenderGraph& GetGraph()const { return graph; }
    const String& GetName()const { return name; }
    U32 GetIndex()const { return index; }

    const std::vector<AccessedTextureResource>& GetInputTextures()const
    {
        return inputTextures;
    }

    const std::vector<RenderTextureResource*>& GetInputColors()const
    {
        return inputColors;
    }

    const std::vector<RenderTextureResource*>& GetOutputColors()const
    {
        return outputColors;
    }

    const std::vector<RenderBufferResource*>& GetInputStorageBuffers()const
    {
        return inputStorageBuffers;
    }

    const std::vector<RenderBufferResource*>& GetOutputStorageBuffers()const
    {
        return outputStorageBuffers;
    }

    const std::vector<RenderTextureResource*>& GetInputAttachments()const
    {
        return inputAttachments;
    }

    const std::vector<RenderTextureResource*>& GetInputStorageTextures()const
    {
        return inputStorageTextures;
    }

    const std::vector<RenderTextureResource*>& GetOutputStorageTextures()const
    {
        return outputStorageTextures;
    }

    std::vector<RenderTextureResource*>& GetInputAttachments()
    {
        return inputAttachments;
    }

    const RenderTextureResource* GetInputDepthStencil()const
    {
        return inputDepthStencil;
    }

    const std::vector<AccessedProxyResource>& GetProxyInputs() const
    {
        return proxyInputs;
    }

    const std::vector<AccessedProxyResource>& GetProxyOutputs() const
    {
        return proxyOutputs;
    }

    RenderTextureResource* GetInputDepthStencil()
    {
        return inputDepthStencil;
    }

    RenderTextureResource* GetOutputDepthStencil()
    {
        return outputDepthStencil;
    }

    const RenderTextureResource* GetOutputDepthStencil()const
    {
        return outputDepthStencil;
    }

    const std::vector<std::pair<RenderTextureResource*, RenderTextureResource*>>& GetFakeResourceAliases() const
    {
        return fakeResourceAlias;
    }

    void SetPhysicalIndex(U32 index)
    {
        physicalIndex = index;
    }

    U32 GetPhysicalIndex()const
    {
        return physicalIndex;
    }

    U32 GetQueue()const
    {
        return queue;
    }

private:
    friend class RenderGraph;

    RenderGraph& graph;
    String name;
    U32 index;
    U32 queue = 0;
    U32 physicalIndex = Unused;

    EnqueuePrepareFunc enqueuePrepareCallback;
    BuildRenderPassFunc buildRenderPassCallback;
    ClearDepthStencilFunc clearDepthStencilCallback;
    ClearColorFunc clearColorCallback;

    RenderTextureResource* inputDepthStencil = nullptr;
    RenderTextureResource* outputDepthStencil = nullptr;
    std::vector<AccessedTextureResource> inputTextures;
    std::vector<RenderTextureResource*> inputColors;
    std::vector<RenderTextureResource*> outputColors;
    std::vector<RenderTextureResource*> inputStorageTextures;
    std::vector<RenderTextureResource*> outputStorageTextures;
    std::vector<RenderBufferResource*> inputStorageBuffers;
    std::vector<RenderBufferResource*> outputStorageBuffers;
    std::vector<RenderTextureResource*> inputAttachments;
    std::vector<AccessedProxyResource> proxyInputs;
    std::vector<AccessedProxyResource> proxyOutputs;

    std::vector<std::pair<RenderTextureResource*, RenderTextureResource*>> fakeResourceAlias;
};

struct DebugRenderPassInfo
{
    String name;
    std::vector<String> reads;
    std::vector<String> writes;
};

class VULKAN_TEST_API RenderGraph
{
public:
    RenderGraph();
    ~RenderGraph();

    void Reset();
    void SetDevice(GPU::DeviceVulkan* device);

    RenderPass& AddRenderPass(const char* name, RenderGraphQueueFlag queueFlag);
    void SetBackBufferSource(const char* name);
    const char* GetBackBufferSource();
    void DisableSwapchain();
    void Bake();
    void SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    void Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle);
    void Log();
    void ExportGraphviz();

    // Editor begin 
    bool IsBaked();
    void GetDebugRenderPassInfos(std::vector<DebugRenderPassInfo>& infos);
    // Editor end

    RenderTextureResource& GetOrCreateTexture(const char* name);
    RenderBufferResource& GetOrCreateBuffer(const char* name);
    RenderResource& GetOrCreateProxy(const char* name);

    GPU::ImageView& GetPhysicalTexture(const RenderTextureResource& res);
    GPU::ImageView* TryGetPhysicalTexture(RenderTextureResource* res);
    GPU::Buffer& GetPhysicalBuffer(const RenderBufferResource& res);
    GPU::Buffer* TryGetPhysicalBuffer(RenderBufferResource* res);

    void SetBackbufferDimension(const ResourceDimensions& dim);

private:
    struct RenderGraphImpl* impl;
};
}