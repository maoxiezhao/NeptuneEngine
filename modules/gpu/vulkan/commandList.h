#pragma once
 
#include "definition.h"
#include "renderPass.h"
#include "frameBuffer.h"
#include "shader.h"
#include "shaderManager.h"
#include "image.h"
#include "buffer.h"
#include "sampler.h"

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

    COMMAND_LIST_DIRTY_DYNAMIC_BITS = COMMAND_LIST_DIRTY_VIEWPORT_BIT | COMMAND_LIST_DIRTY_SCISSOR_BIT
};
using CommandListDirtyFlags = U32;

struct CompilePipelineState
{
	ShaderProgram* shaderProgram = nullptr;
    BlendState blendState = {};
    RasterizerState rasterizerState = {};
    DepthStencilState depthStencilState = {};
    VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VertexAttribState attribs[VULKAN_NUM_VERTEX_ATTRIBS];
    VkDeviceSize strides[VULKAN_NUM_VERTEX_BUFFERS];
    VkVertexInputRate inputRates[VULKAN_NUM_VERTEX_BUFFERS];

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

class CommandList : public Util::IntrusivePtrEnabled<CommandList, CommandListDeleter>
{
private:
    friend struct CommandListDeleter;
    friend class Util::ObjectPool<CommandList>;

    VkViewport viewport = {};
    VkRect2D scissor = {};
    VkPipeline currentPipeline = VK_NULL_HANDLE;               // ����ʵ��
    VkPipelineLayout currentPipelineLayout = VK_NULL_HANDLE;   // ������Դ�ֲ�

    DeviceVulkan& device;
    VkCommandBuffer cmd;
    QueueType type;
    VkPipelineStageFlags swapchainStages = 0;
    U32 activeVBOs = 0;

    // render pass runtime 
    FrameBuffer* frameBuffer = nullptr;
    const ImageView* frameBufferAttachments[VULKAN_NUM_ATTACHMENTS + 1] = {};
    RenderPass* renderPass = nullptr;
    const RenderPass* compatibleRenderPass = nullptr;

    CompilePipelineState pipelineState = {};
    PipelineLayout* currentLayout = nullptr;
    ResourceBindings bindings;
    VkDescriptorSet bindlessSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    VkDescriptorSet allocatedSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    U32 dirtySets = 0;

public:
    CommandList(DeviceVulkan& device_, VkCommandBuffer buffer_, QueueType type_);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void BindPipelineState(const CompilePipelineState& pipelineState_);
    void BindVertexBuffers();
    void BindIndexBuffers();
    
    void PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range);
    void CopyToImage(const ImagePtr& image, const BufferPtr& buffer, U32 numBlits, const VkBufferImageCopy* blits);
    void CopyBuffer(const BufferPtr& dst, const BufferPtr& src);
    void CopyBuffer(const BufferPtr& dst, VkDeviceSize dstOffset, const BufferPtr& src, VkDeviceSize srcOffset, VkDeviceSize size);
    void FillBuffer(const BufferPtr& buffer, U32 value);
    void SetBindless(U32 set, VkDescriptorSet descriptorSet);
    void SetSampler(U32 set, U32 binding, const Sampler& sampler);
    void SetSampler(U32 set, U32 binding, StockSampler type);

    void Draw(U32 vertexCount, U32 vertexOffset = 0);
    void DrawIndexed(U32 indexCount, U32 firstIndex = 0, U32 vertexOffset = 0);

    void ImageBarrier(const ImagePtr& image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess);
    void Barrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, unsigned imageBarrierCount, const VkImageMemoryBarrier* imageBarriers);
    void BeginEvent(const char* name);
    void EndEvent();

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

public:
    void ClearPipelineState();
    void SetDefaultOpaqueState();
    void SetProgram(ShaderProgram* program);
    void SetProgram(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines = {});

private:
    friend class DeviceVulkan;

    void BeginGraphicsContext();
    void EndCommandBuffer();

    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    void FlushDescriptorSets();
    void FlushDescriptorSet(U32 set);
    void UpdateGraphicsPipelineHash(CompilePipelineState pipeline);

    VkPipeline BuildGraphicsPipeline(const CompilePipelineState& pipelineState);
    VkPipeline BuildComputePipeline(const CompilePipelineState& pipelineState);

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
using CommandListPtr = Util::IntrusivePtr<CommandList>;

}