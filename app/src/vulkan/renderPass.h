#pragma once

#include "image.h"

class DeviceVulkan;

struct RenderPassInfo
{
    const ImageView* mColorAttachments[VULKAN_NUM_ATTACHMENTS];
    const ImageView* mDepthStencil = nullptr;
    uint32_t mNumColorAttachments = 0;
    uint32_t mClearAttachments = 0;
    uint32_t mLoadAttachments = 0;
    uint32_t mStoreAttachments = 0;
    VkRect2D mRenderArea = { { 0, 0 }, { UINT32_MAX, UINT32_MAX } };
};

class RenderPass
{
private:
    DeviceVulkan& mDevice;
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    uint64_t mHash;

public:
    RenderPass(DeviceVulkan& device);
    ~RenderPass();

    void SetHash(uint64_t hash)
    {
        mHash = hash;
    }
    uint64_t GetHash()
    {
        return mHash;
    }
};
