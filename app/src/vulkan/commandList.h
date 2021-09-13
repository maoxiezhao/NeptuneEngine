#pragma once
 
#include "definition.h"
#include "renderPass.h"
#include "frameBuffer.h"

class DeviceVulkan;

enum CommandListDirtyBits
{
	COMMAND_LIST_DIRTY_PIPELINE_BIT = 1 << 0,
};
using CommandListDirtyFlags = uint32_t;

struct CompilePipelineState
{
	ShaderProgram mShaderProgram;
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
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;
    CompilePipelineState mPipelineState = {};

    DeviceVulkan& mDevice;
    VkCommandBuffer mCmd;
    QueueType mType;
    VkPipelineStageFlags mSwapchainStages = 0;

    // render pass runtime 
    FrameBuffer* mFrameBuffer = nullptr;
    RenderPass* mRenderPass = nullptr;

public:
    CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void BindPipelineState(const CompilePipelineState& pipelineState);

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

private:
    friend class DeviceVulkan;
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

