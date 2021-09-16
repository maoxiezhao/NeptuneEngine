#pragma once
 
#include "definition.h"
#include "renderPass.h"
#include "frameBuffer.h"
#include "shader.h"

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
};
using CommandListDirtyFlags = uint32_t;

struct CompilePipelineState
{
	ShaderProgram* mShaderProgram = nullptr;
    BlendState mBlendState = {};
    RasterizerState mRasterizerState = {};
    DepthStencilState mDepthStencilState = {};
    VkPrimitiveTopology mTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VertexAttribState attribs[VULKAN_NUM_VERTEX_ATTRIBS];
    VkDeviceSize strides[VULKAN_NUM_VERTEX_BUFFERS];
    VkVertexInputRate inputRates[VULKAN_NUM_VERTEX_BUFFERS];

    uint32_t mSubpassIndex;
    uint64_t mHash;
    VkPipelineCache mCache;
};

struct CommandPool
{
public:
    CommandPool(DeviceVulkan* device, uint32_t queueFamilyIndex);
    ~CommandPool();

    CommandPool(CommandPool&&) noexcept;
    CommandPool& operator=(CommandPool&&) noexcept;

    CommandPool(const CommandPool& rhs) = delete;
    void operator=(const CommandPool& rhs) = delete;

    VkCommandBuffer RequestCommandBuffer();
    void BeginFrame();

private:
    uint32_t mUsedIndex = 0;
    DeviceVulkan* mDevice;
    VkCommandPool mPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mBuffers;
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

    VkViewport mViewport = {};
    VkRect2D mScissor = {};
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;               // 管线实例
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;   // 管线资源分布
    CompilePipelineState mPipelineState = {};

    DeviceVulkan& mDevice;
    VkCommandBuffer mCmd;
    QueueType mType;
    VkPipelineStageFlags mSwapchainStages = 0;

    // render pass runtime 
    FrameBuffer* mFrameBuffer = nullptr;
    RenderPass* mRenderPass = nullptr;
    RenderPass* mCompatibleRenderPass = nullptr;

public:
    CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void BindPipelineState(const CompilePipelineState& pipelineState);
    void BindVertexBuffers();
    void BindIndexBuffers();

    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, uint32_t vertexOffset = 0);

    QueueType GetQueueType()const
    {
        return mType;
    }

    VkCommandBuffer GetCommandBuffer()const
    {
        return mCmd;
    }

    VkPipelineStageFlags GetSwapchainStages()const
    {
        return mSwapchainStages;
    }

    void SetSwapchainStages(VkPipelineStageFlags stages)
    {
        mSwapchainStages |= stages;
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

    VkPipeline BuildGraphicsPipeline(const CompilePipelineState& pipelineState);
    VkPipeline BuildComputePipeline(const CompilePipelineState& pipelineState);

    CommandListDirtyFlags mDirty = 0;
    void SetDirty(CommandListDirtyFlags flags)
    {
        mDirty |= flags;
    }
   
    bool IsDirty(CommandListDirtyFlags flags)
    {
        return (mDirty & flags) != 0;
    }

    bool IsDirtyAndClear(CommandListDirtyFlags flags)
    {
        auto ret = (mDirty & flags) != 0;
        mDirty &= ~flags;
        return ret;
    }
};
using CommandListPtr = Util::IntrusivePtr<CommandList>;

