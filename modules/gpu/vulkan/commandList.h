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
#include "queryPool.h"

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
    COMMAND_LIST_DIRTY_STENCIL_REFERENCE_BIT =  1 << 5,

    COMMAND_LIST_DIRTY_DYNAMIC_BITS = COMMAND_LIST_DIRTY_VIEWPORT_BIT | 
                                      COMMAND_LIST_DIRTY_SCISSOR_BIT |
                                      COMMAND_LIST_DIRTY_STENCIL_REFERENCE_BIT
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

struct PipelineStateDesc
{
    const Shader* shaders[static_cast<U32>(ShaderStage::Count)];
    BlendState blendState = {};
    RasterizerState rasterizerState = {};
    DepthStencilState depthStencilState = {};
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
    VkPipelineCache cache = VK_NULL_HANDLE;
    bool isOwnedByCommandList = true;
};

struct DynamicState
{
    U8 frontReference = 0;
    U8 backReference = 0;
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
    U32 dirtySetsDynamic = 0; // Used for constant buffer dynamic offset
    U32 dirtyVbos = 0;

    BufferBlock vboBlock;
    BufferBlock iboBlock;
    BufferBlock uboBlock;
    BufferBlock stagingBlock;
    BufferBlock storageBlock;

    // render pass runtime 
    FrameBuffer* frameBuffer = nullptr;
    const ImageView* frameBufferAttachments[VULKAN_NUM_ATTACHMENTS + 1] = {};
    RenderPass* renderPass = nullptr;
    const RenderPass* compatibleRenderPass = nullptr;
    VkSubpassContents subpassContents;
    DynamicState dynamicState = {};

    Platform::ThreadID threadID = 0;
    bool isEnded = false;

public:
    CommandList(DeviceVulkan& device_, VkCommandBuffer buffer_, QueueType type_, VkPipelineCache cache_);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo, VkSubpassContents contents = VK_SUBPASS_CONTENTS_INLINE);
    void EndRenderPass();
#if 0
    void BindPipelineState(const CompiledPipelineState& pipelineState_);
#endif
    void* AllocateVertexBuffer(U32 binding, VkDeviceSize size, VkDeviceSize stride, VkVertexInputRate inputRate);
    void* AllocateIndexBuffer(VkDeviceSize size, VkIndexType indexType);
    void SetVertexAttribute(U32 attribute, U32 binding, VkFormat format, VkDeviceSize offset);
    void BindVertexBuffer(const BufferPtr& buffer, U32 binding, VkDeviceSize offset, VkDeviceSize stride, VkVertexInputRate inputRate);
    void BindIndexBuffer(const BufferPtr& buffer, VkDeviceSize offset, VkIndexType indexType);
    void BindConstantBuffer(const BufferPtr& buffer,U32 set, U32 binding, VkDeviceSize offset, VkDeviceSize range);
    void PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range);
    void* AllocateConstant(U32 set, U32 binding, VkDeviceSize size);
    BufferBlockAllocation AllocateStorageBuffer(VkDeviceSize size);

    template<typename T>
    T* AllocateConstant(U32 set, U32 binding)
    {
        return static_cast<T*>(AllocateConstant(set, binding, sizeof(T)));
    }

    template<typename T>
    void BindConstant(const T& data, U32 set, U32 binding)
    {
        T* mem = static_cast<T*>(AllocateConstant(set, binding, sizeof(T)));
        memcpy(mem, &data, sizeof(T));
    }

    void UpdateBuffer(const Buffer* buffer, const void* data, VkDeviceSize size)
    {
        if (buffer == nullptr || data == nullptr)
            return;
        void* mem = UpdateBuffer(*buffer, 0, size);
        memcpy(mem, data, size);
    }
    void* UpdateBuffer(const Buffer& buffer, VkDeviceSize offset, VkDeviceSize size);
    void* UpdateImage(const Image& image, const VkOffset3D& offset, const VkExtent3D& extent, U32 rowLength, U32 imageHeight, const VkImageSubresourceLayers& subresource);
    void* UpdateImage(const Image& image, U32 rowLenght = 0, U32 imageHeight = 0);
    void CopyToImage(const Image& image, const Buffer& buffer, VkDeviceSize bufferOffset, const VkOffset3D& offset, const VkExtent3D& extent, unsigned rowLength, unsigned sliceHeight, const VkImageSubresourceLayers& subresource);
    void CopyToImage(const Image& image, const Buffer& buffer, U32 numBlits, const VkBufferImageCopy* blits);
    void CopyBuffer(const Buffer& dst, const Buffer& src);
    void CopyBuffer(const Buffer& dst, VkDeviceSize dstOffset, const Buffer& src, VkDeviceSize srcOffset, VkDeviceSize size);
    void FillBuffer(const BufferPtr& buffer, U32 value);
    void SetBindless(U32 set, VkDescriptorSet descriptorSet);
    void SetSampler(U32 set, U32 binding, const Sampler& sampler);
    void SetSampler(U32 set, U32 binding, StockSampler type);
    void SetTexture(U32 set, U32 binding, const ImageView& imageView);
    void SetStorageTexture(U32 set, U32 binding, const ImageView& view);
    void SetStorageBuffer(U32 set, U32 binding, const Buffer& buffer);
    void SetStorageBuffer(U32 set, U32 binding, const Buffer& buffer, VkDeviceSize offset, VkDeviceSize range);
    void SetRasterizerState(const RasterizerState& state);
    void SetBlendState(const BlendState& state);
    void SetDepthStencilState(const DepthStencilState& state);
    void SetPipelineState(const PipelineStateDesc& desc);
    void SetScissor(const VkRect2D& rect);
    void SetViewport(const VkViewport& viewport_);
    void SetViewport(const Viewport& viewport_);
    void SetStencilRef(U8 ref, StencilFace face);
    void NextSubpass(VkSubpassContents contents);

    void Draw(U32 vertexCount, U32 vertexOffset = 0);
    void DrawInstanced(U32 vertexCount, U32 instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation);
    void DrawIndexed(U32 indexCount, U32 firstIndex = 0, U32 vertexOffset = 0);
    void DrawIndexedInstanced(U32 indexCount, U32 instanceCount, U32 startIndexLocation, U32 baseVertexLocation, U32 startInstanceLocation);

    void Dispatch(U32 groupsX, U32 groupsY, U32 groupsZ);
    void DispatchIndirect(const Buffer& buffer, U32 offset);

    void BufferBarrier(const Buffer& buffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess);
    void BufferBarrier(const Buffer& buffer, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void ImageBarrier(const Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, unsigned bufferBarrierCount, const VkBufferMemoryBarrier* bufferBarriers, unsigned imageBarrierCount, const VkImageMemoryBarrier* imageBarriers);
    void CompleteEvent(const Event& ent);
    void WaitEvents(U32 numEvents, VkEvent* events, 
                   VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
                   U32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, 
                   U32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, 
                   U32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers);
    void BeginEvent(const char* name);
    void EndEvent();

    QueryPoolResultPtr WriteTimestamp(VkPipelineStageFlagBits stage);

    // Used to end command buffer in a thread, and submitting in a different thread.
    void EndCommandBufferForThread();

    void SetThreadID(Platform::ThreadID threadIndex_)
    {
        threadID = threadIndex_;
    }

    Platform::ThreadID GetThreadID()const
    {
        return threadID;
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

    void SetStorageBlock(BufferBlock block)
    {
        storageBlock = block;
    }

    BufferBlock GetStorageBlock()const
    {
        return storageBlock;
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
    void SetShaderProgram(ShaderProgram* program);
    void SetProgramShaders(const Shader* shaders[static_cast<U32>(ShaderStage::Count)]);
    void SetProgram(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines = {});
    void SetProgram(const Shader* vertex, const Shader* fragment);
    void SetProgram(const Shader* compute);

private:
    friend class DeviceVulkan;

    void ResetCommandContext();
    void EndCommandBuffer();

    void SetTextureImpl(U32 set, U32 binding, VkImageView imageView, VkImageLayout layout, U64 cookie, DescriptorSetType setType);

    bool FlushRenderState();
    bool FlushComputeState();
    bool FlushGraphicsPipeline();
    bool FlushComputePipeline();
    void FlushDescriptorSets();
    void FlushDescriptorSet(U32 set);
    void FlushDescriptorDynamicSet(U32 set);
    void UpdateGraphicsPipelineHash(CompiledPipelineState& pipeline, U32& activeVbos);
    void UpdateComputePipelineHash(CompiledPipelineState& pipeline);

    VkPipeline BuildGraphicsPipeline(const CompiledPipelineState& pipelineState);
    VkPipeline BuildComputePipeline(const CompiledPipelineState& pipelineState);

    void BeginCompute();
    void BeginGraphics();

    bool isCompute = true;
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