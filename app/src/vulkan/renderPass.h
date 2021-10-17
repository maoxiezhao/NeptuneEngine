#pragma once

#include "image.h"

namespace GPU
{

class DeviceVulkan;

struct RenderPassInfo
{
    const ImageView* colorAttachments[VULKAN_NUM_ATTACHMENTS];
    const ImageView* depthStencil = nullptr;
    uint32_t numColorAttachments = 0;
    uint32_t clearAttachments = 0;
    uint32_t loadAttachments = 0;
    uint32_t storeAttachments = 0;
    VkRect2D renderArea = { { 0, 0 }, { UINT32_MAX, UINT32_MAX } };

    struct SubPass
    {
        uint32_t colorAttachments[VULKAN_NUM_ATTACHMENTS] = {};
        uint32_t inputAttachments[VULKAN_NUM_ATTACHMENTS] = {};
        uint32_t resolveAttachments[VULKAN_NUM_ATTACHMENTS] = {};
        uint32_t numColorAattachments = 0;
        uint32_t numInputAttachments = 0;
        uint32_t numResolveAttachments = 0;
        DepthStencilMode depthStencilMode = DepthStencilMode::ReadWrite;
    };
    const SubPass* subPasses = nullptr;
    unsigned numSubPasses = 0;
};

class RenderPass : public HashedObject<RenderPass>
{
private:
    DeviceVulkan& device;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    uint64_t hash;
	VkFormat colorAttachments[VULKAN_NUM_ATTACHMENTS] = {};
	VkFormat depthStencil = VK_FORMAT_UNDEFINED;

    struct InitedSubpassInfo
    {
        VkAttachmentReference colorAttachments[VULKAN_NUM_ATTACHMENTS];
        unsigned numColorAttachments;
        VkAttachmentReference inputAttachments[VULKAN_NUM_ATTACHMENTS];
        unsigned numInputAttachments;
        VkAttachmentReference depthStencilAttachment;

        uint32_t samples;
    };
	std::vector<InitedSubpassInfo> initedSubpassInfos;

    void SetupSubPasses(const VkRenderPassCreateInfo& info);

public:
    RenderPass(DeviceVulkan& device_, const RenderPassInfo& info);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
	void operator=(const RenderPass&) = delete;

    const VkRenderPass GetRenderPass() const
    {
        return renderPass;
    }

    U32 GetNumColorAttachments(U32 subpass)const
    {
        ASSERT(subpass < initedSubpassInfos.size());
        return initedSubpassInfos[subpass].numColorAttachments;
    }

    VkAttachmentReference GetColorAttachment(U32 subpass, U32 colorIndex)const
    {
        ASSERT(subpass < initedSubpassInfos.size());
        ASSERT(colorIndex < initedSubpassInfos[subpass].numColorAttachments);
        return initedSubpassInfos[subpass].colorAttachments[colorIndex];
    }

    bool HasDepth(U32 subpass) const
    {
        ASSERT(subpass < initedSubpassInfos.size());
        return initedSubpassInfos[subpass].depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED && IsFormatHasDepth(depthStencil);
    }

    bool HasStencil(U32 subpass) const
    {
        ASSERT(subpass < initedSubpassInfos.size());
        return initedSubpassInfos[subpass].depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED && IsFormatHasStencil(depthStencil);
    }
};

}
