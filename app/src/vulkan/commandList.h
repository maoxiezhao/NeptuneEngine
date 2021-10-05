#pragma once
 
#include "definition.h"
#include "renderPass.h"
#include "frameBuffer.h"
#include "shader.h"

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
using CommandListDirtyFlags = uint32_t;

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

    uint32_t subpassIndex = 0;
    uint64_t hash = 0;
    VkPipelineCache cache;
    bool isOwnedByCommandList = true;
};

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device_, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(CommandPool&&) noexcept;
    CommandPool& operator=(CommandPool&&) noexcept;

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

    VkCommandBuffer RequestCommandBuffer();
    void BeginFrame();

private:
    uint32_t usedIndex = 0;
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
    U32 dirtySets = 0;

    // render pass runtime 
    FrameBuffer* frameBuffer = nullptr;
    RenderPass* renderPass = nullptr;
    const RenderPass* compatibleRenderPass = nullptr;
    CompilePipelineState pipelineState = {};
    PipelineLayout* currentLayout = nullptr;
    ResourceBindings bindings;
    VkDescriptorSet bindlessSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};
    VkDescriptorSet allocatedSets[VULKAN_NUM_DESCRIPTOR_SETS] = {};


public:
    CommandList(DeviceVulkan& device_, VkCommandBuffer buffer_, QueueType type_);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void BindPipelineState(const CompilePipelineState& pipelineState_);
    void BindVertexBuffers();
    void BindIndexBuffers();

    void PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range);

    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t vertexOffset = 0);

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

private:
    friend class DeviceVulkan;

    void BeginGraphicsContext();
    void EndCommandBuffer();

    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    void FlushDescriptorSets();
    void FlushDescriptorSet(U32 set);

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