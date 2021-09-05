#pragma once

#include "definition.h"
#include "renderPass.h"

class DeviceVulkan;

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
public:
    VkViewport mViewport;
    VkRect2D mScissor;
    VkPipeline mCurrentPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mCurrentPipelineLayout = VK_NULL_HANDLE;
    bool mIsDirtyPipeline = true;
    PipelineStateDesc mPipelineStateDesc = {};

    DeviceVulkan& mDevice;
    VkCommandBuffer mCmd;
    QueueType mType;

    // render pass runtime 
    FrameBuffer* mFrameBuffer = nullptr;
    RenderPass* mRenderPass = nullptr;

public:
    CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type);
    ~CommandList();

    void BeginRenderPass(const RenderPassInfo& renderPassInfo);
    void EndRenderPass();
    void EndCommandBuffer();

private:
    bool FlushRenderState();
    bool FlushGraphicsPipeline();
    VkPipeline BuildGraphicsPipeline(const PipelineStateDesc& pipelineStateDesc);
};
using CommandListPtr = Util::IntrusivePtr<CommandList>;

