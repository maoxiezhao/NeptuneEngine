#pragma once

#include "gpu\vulkan\device.h"
#include "core\jobsystem\jobsystem.h"
#include "core\utils\string.h"

#include <unordered_set>

namespace VulkanTest
{
class RenderGraph;

enum class RenderGraphQueueFlag
{
    Graphics,
    Compute,
    AsyncGraphcs,
    AsyncCompute
};

enum class AttachmentSizeType
{
    Absolute,
    SwapchainRelative,
};

enum class ResourceType
{
    Texture,
    Buffer,
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
};

class VULKAN_TEST_API RenderResource
{
public:
    RenderResource(U32 index_, ResourceType resType_) :  index(index_), resType(resType_) {}
    virtual ~RenderResource() {};

    const std::unordered_set<U32>& GetWrittenPasses()const
    {
        return writtenPasses;
    }

    const std::unordered_set<U32>& GetReadPasses()const
    {
        return readPasses;
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

    ResourceType GetResourceType()const
    {
        return resType;
    }

protected:
    friend class RenderGraph;

    U32 index;
    String name;
    ResourceType resType;
    std::unordered_set<U32> writtenPasses;
    std::unordered_set<U32> readPasses;
    U32 physicalIndex = Unused;
    U32 usedQueues = 0;
};

class VULKAN_TEST_API RenderTextureResource : public RenderResource
{
public:
    RenderTextureResource(U32 index_) : RenderResource(index_, ResourceType::Texture) {}

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
    RenderBufferResource(U32 index_) : RenderResource(index_, ResourceType::Buffer) {}

    void SetBufferUsage(VkBufferUsageFlags flags)
    {
        usage |= flags;
    }

private:
    VkBufferUsageFlags usage = 0;
};

using BuildRenderPassFunc = std::function<void(GPU::CommandList&)>;
using ClearDepthStencilFunc = std::function<bool(VkClearDepthStencilValue* value)>;
using ClearColorFunc = std::function<bool(U32 index, VkClearColorValue* value)>;

class VULKAN_TEST_API RenderPass
{
public:
    RenderPass(RenderGraph& graph_, U32 index_, U32 queue_);
    ~RenderPass();

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
    
    RenderTextureResource& ReadTexture(const char* name);
    RenderTextureResource& ReadDepthStencil(const char* name);
    RenderTextureResource& WriteColor(const char* name, const AttachmentInfo& info);
    RenderTextureResource& WriteDepthStencil(const char* name, const AttachmentInfo& info);

    RenderBufferResource& ReadStorageBuffer(const char* name);
    RenderBufferResource& WriteStorageBuffer(const char* name, const BufferInfo& info, const char* input);

    const RenderGraph& GetGraph()const { return graph; }
    const String& GetName()const { return name; }
    U32 GetIndex()const { return index; }

    const std::vector<RenderTextureResource*>& GetInputTextures()const
    {
        return inputTextures;
    }

    const std::vector<RenderTextureResource*>& GetOutputColors()const
    {
        return outputColors;
    }

    const std::vector<RenderBufferResource*>& GetInputBuffers()const
    {
        return inputBuffers;
    }

    const std::vector<RenderBufferResource*> GetOutputBuffers()const
    {
        return outputBuffers;
    }

    const std::vector<RenderTextureResource*>& GetInputAttachments()const
    {
        return inputAttachments;
    }

    std::vector<RenderTextureResource*>& GetInputAttachments()
    {
        return inputAttachments;
    }

    const RenderTextureResource* GetInputDepthStencil()const
    {
        return inputDepthStencil;
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

    enum { Unused = ~0u };

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
    BuildRenderPassFunc buildRenderPassCallback;
    ClearDepthStencilFunc clearDepthStencilCallback;
    ClearColorFunc clearColorCallback;

    RenderTextureResource* inputDepthStencil;
    RenderTextureResource* outputDepthStencil;
    std::vector<RenderTextureResource*> inputTextures;
    std::vector<RenderTextureResource*> outputColors;
    std::vector<RenderBufferResource*> inputBuffers;
    std::vector<RenderBufferResource*> outputBuffers;
    std::vector<RenderTextureResource*> inputAttachments;
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
    void Bake();
    void Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle);
    void Log();

    RenderTextureResource& GetOrCreateTexture(const char* name);
    RenderBufferResource& GetOrCreateBuffer(const char* name);

private:
    struct RenderGraphImpl* impl;
};
}