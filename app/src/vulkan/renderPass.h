#pragma once

#include "image.h"

class DeviceVulkan;

struct SubPassInfo
{
    uint32_t mColorAttachments[VULKAN_NUM_ATTACHMENTS];
	uint32_t mInputAttachments[VULKAN_NUM_ATTACHMENTS];
	uint32_t mResolveAttachments[VULKAN_NUM_ATTACHMENTS];
    uint32_t mNumColorAattachments = 0;
    uint32_t mNumInputAttachments = 0;
    uint32_t mNumResolveAttachments = 0;
};

struct RenderPassInfo
{
    const ImageView* mColorAttachments[VULKAN_NUM_ATTACHMENTS];
    const ImageView* mDepthStencil = nullptr;
    uint32_t mNumColorAttachments = 0;
    uint32_t mClearAttachments = 0;
    uint32_t mLoadAttachments = 0;
    uint32_t mStoreAttachments = 0;
    VkRect2D mRenderArea = { { 0, 0 }, { UINT32_MAX, UINT32_MAX } };

    const SubPassInfo* mSubPasses = nullptr;
    unsigned mNumSubPasses = 0;
};

struct Subpass
{
	VkAttachmentReference colorAttachments[VULKAN_NUM_ATTACHMENTS];
	unsigned numColorAttachments;
	VkAttachmentReference inputAttachments[VULKAN_NUM_ATTACHMENTS];
	unsigned numInputAttachments;
	VkAttachmentReference depthStencilAttachment;

    uint32_t mSamples;
};

class RenderPass : public HashedObject<RenderPass>
{
private:
    DeviceVulkan& mDevice;
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    uint64_t mHash;
	VkFormat mColorAttachments[VULKAN_NUM_ATTACHMENTS] = {};
	VkFormat mDepthStencil = VK_FORMAT_UNDEFINED;
	std::vector<Subpass> mSubpasses;

public:
    RenderPass(DeviceVulkan& device, const RenderPassInfo& info);
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
	void operator=(const RenderPass&) = delete;

    const VkRenderPass GetRenderPass() const
    {
        return mRenderPass;
    }

    U32 GetNumColorAttachments(U32 subpass)const
    {
        ASSERT(subpass < mSubpasses.size());
        return mSubpasses[subpass].numColorAttachments;
    }

    VkAttachmentReference GetColorAttachment(U32 subpass, U32 colorIndex)const
    {
        ASSERT(subpass < mSubpasses.size());
        ASSERT(colorIndex < mSubpasses[subpass].numColorAttachments);
        return mSubpasses[subpass].colorAttachments[colorIndex];
    }

    bool HasDepth(U32 subpass) const
    {
        ASSERT(subpass < mSubpasses.size());
        return mSubpasses[subpass].depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED && IsFormatHasDepth(mDepthStencil);
    }

    bool HasStencil(U32 subpass) const
    {
        ASSERT(subpass < mSubpasses.size());
        return mSubpasses[subpass].depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED && IsFormatHasStencil(mDepthStencil);
    }
};
