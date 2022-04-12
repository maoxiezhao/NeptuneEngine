#pragma once
 
#include "definition.h"
#include "renderPass.h"
#include "frameBuffer.h"
#include "shader.h"
#include "shaderManager.h"
#include "image.h"
#include "buffer.h"
#include "bufferPool.h"
#include "sampler.h"
#include "event.h"

namespace VulkanTest
{
namespace GPU
{

class DeviceVulkan;

#define COMPARE_OP_BITS 3
#define STENCIL_OP_BITS 3
#define BLEND_FACTOR_BITS 5
#define BLEND_OP_BITS 3
#define CULL_MODE_BITS 2
#define FRONT_FACE_BITS 1

enum CommandListDirtyBits
{
	COMMAND_LIST_DIRTY_PIPELINE_BIT = 1 << 0,
    COMMAND_LIST_DIRTY_VIEWPORT_BIT = 1 << 1,
    COMMAND_LIST_DIRTY_SCISSOR_BIT = 1 << 2,
    COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT = 1 << 3,

    COMMAND_LIST_DIRTY_STATIC_VERTEX_BIT = 1 << 4,

    COMMAND_LIST_DIRTY_DYNAMIC_BITS = COMMAND_LIST_DIRTY_VIEWPORT_BIT | COMMAND_LIST_DIRTY_SCISSOR_BIT
};
using CommandListDirtyFlags = U32;

struct VertexBindingState
{
    VkBuffer buffers[VULKAN_NUM_VERTEX_BUFFERS];
    VkDeviceSize offsets[VULKAN_NUM_VERTEX_BUFFERS];
    VkDeviceSize strides[VULKAN_NUM_VERTEX_BUFFERS];
    VkVertexInputRate inputRate[VULKAN_NUM_VERTEX_BUFFERS];
};

struct IndexBindingState
{
    VkBuffer buffer;
	VkDeviceSize offset;
	VkIndexType indexType;
};

struct CompiledPipelineState
{
	ShaderProgram* shaderProgram = nullptr;
    BlendState blendState = {};
    RasterizerState rasterizerState = {};
    DepthStencilState depthStencilState = {};
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VertexAttribState attribs[VULKAN_NUM_VERTEX_ATTRIBS];

    U32 subpassIndex = 0;
    uint64_t hash = 0;
    VkPipelineCache cache;
    bool isOwnedByCommandList = true;
};

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device_, U32 queueFamilyIndex);
    ~CommandPool();

    CommandPool(CommandPool&&) noexcept;
    CommandPool& operator=(CommandPool&&) noexcept;

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

    VkCommandBuffer RequestCommandBuffer();
    void BeginFrame();

private:
    U32 usedIndex = 0;
    DeviceVulkan* device;
    VkCommandPool pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> buffers;
};

class CommandList;
struct CommandListDeleter
{
    void operator()(CommandList* cmd);
};

class CommandList : public IntrusivePtrEnabled<CommandList, CommandListDeleter>
{
private:
    friend struct CommandListDeleter;
    friend class Util::ObjectPool<CommandList>;

    VkViewport viewport = {};
    VkRect2D scissor = {};
    VkPipeline currentPipeline = VK_NULL_HANDLE;              
    VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;

    DeviceVulkan& device;
    VkCommandBuffer cmd;
    QueueType type;
    VkPipelineStageFlags swapchainStages = 0;
    U32 activeVBOs = 0;
    VertexBindingState vbos;
    IndexBindingState indexState;

    CompiledPipelineState pipelineState = {};
    PipelineLayout* currentLayout = nullptr;
    ResourceBindings bindings;
    VkDescriptorSet bindlessSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    VkDescriptorSet allocatedSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    U32 dirtySets = 0;
    U32 dirtyVbos = 0;

    BufferBlock vboBlock;
    BufferBlock iboBlock;

    // render pass runtime 
    FrameBuffer* frameBuffer = nullptr;
    const ImageView* frameBufferAttachments[VULKAN_NUM_ATTACHMENTS + 1] = {};
    RenderPass* renderPass = nullptr;
    const RenderPass* compatibleRenderPass = nullptr;
    VkSubpassContents subpassContents;

    U32 threadIndex = 0;
    bool isEnded = false;

public:
    CommandList(DeviceVulkan& device_, VkCommandBuffer buffer_, QueueType type_);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);
    void EndRenderPass();
    void BindPipelineState(const CompiledPipelineState& pipelineState_);
    void* AllocateVertexBuffer(U32 binding, VkDeviceSize size, VkDeviceSize stride, VkVertexInputRate inputRate);
    void* AllocateIndexBuffer(VkDeviceSize size, VkIndexType indexType);
    void SetVertexAttribute(U32 attribute, U32 binding, VkFormat format, VkDeviceSize offset);
    void BindVertexBuffer(const BufferPtr& buffer, U32 binding, VkDeviceSize offset, VkDeviceSize stride, VkVertexInputRate inputRate);
    void BindIndexBuffer(const BufferPtr& buffer, VkDeviceSize offset, VkIndexType indexType);
    
    void PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range);
    void CopyToImage(const ImagePtr& image, const BufferPtr& buffer, U32 numBlits, const VkBufferImageCopy* blits);
    void CopyBuffer(const BufferPtr& dst, const BufferPtr& src);
    void CopyBuffer(const BufferPtr& dst, VkDeviceSize dstOffset, const BufferPtr& src, VkDeviceSize srcOffset, VkDeviceSize size);
    void FillBuffer(const BufferPtr& buffer, U32 value);
    void SetBindless(U32 set, VkDescriptorSet descriptorSet);
    void SetSampler(U32 set, U32 binding, const Sampler& sampler);
    void SetSampler(U32 set, U32 binding, StockSampler type);
    void SetTexture(U32 set, U32 binding, const ImageView& imageView);
    void SetRasterizerState(const RasterizerState& state);
    void SetBlendState(const BlendState& state);
    void NextSubpass(VkSubpassContents contents);

    void Draw(U32 vertexCount, U32 vertexOffset = 0);
    void DrawIndexed(U32 indexCount, U32 firstIndex = 0, U32 vertexOffset = 0);

    void ImageBarrier(const ImagePtr& image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, unsigned imageBarrierCount, const VkImageMemoryBarrier* imageBarriers);
    void CompleteEvent(const Event& ent);
    void WaitEvents(U32 numEvents, VkEvent* events, 
                   VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
                   U32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, 
                   U32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, 
                   U32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
    void BeginEvent(const char* name);
    void EndEvent();

    // Used to end command buffer in a thread, and submitting in a different thread.
    void EndCommandBufferForThread();

    void SetThreadIndex(U32 threadIndex_)
    {
        threadIndex = threadIndex_;
    }

    U32 GetThreadIndex()const
    {
        return threadIndex;
    }

    QueueType GetQueueType()const
    {
        return type;
    }

    VkCommandBuffer GetCommandBuffer()const
    {
        return cmd;
    }

    VkPipelineStageFlags GetSwapchainStages()const
    {
        return swapchainStages;
    }

    void SetSwapchainStages(VkPipelineStageFlags stages)
    {
        swapchainStages |= stages;
    }

    DeviceVulkan& GetDevice()
    {
        return device;
    }

public:
    void ClearPipelineState();
    void SetDefaultOpaqueState();
    void SetDefaultTransparentState();
    void SetPrimitiveTopology(VkPrimitiveTopology topology);
    void SetProgram(ShaderProgram* program);
    void SetProgram(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines = {});

private:
    friend class DeviceVulkan;

    void ResetCommandContext();
    void EndCommandBuffer();

    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    void FlushDescriptorSets();
    void FlushDescriptorSet(U32 set);
    void UpdateGraphicsPipelineHash(CompiledPipelineState& pipeline, U32& activeVbos);

    VkPipeline BuildGraphicsPipeline(const CompiledPipelineState& pipelineState);
    VkPipeline BuildComputePipeline(const CompiledPipelineState& pipelineState);

    CommandListDirtyFlags dirty = 0;
    void SetDirty(CommandListDirtyFlags flags)
    {
        dirty |= flags;
    }
   
    bool IsDirty(CommandListDirtyFlags flags)
    {
        return (dirty & flags) != 0;
    }

    bool IsDirtyAndClear(CommandListDirtyFlags flags)
    {
        auto ret = (dirty & flags) != 0;
        dirty &= ~flags;
        return ret;
    }
};
using CommandListPtr = IntrusivePtr<CommandList>;

}
}