#pragma once

#include "image.h"

class DeviceVulkan;

struct RenderPassInfo
{
    const ImageView* mColorAttachments[VULKAN_NUM_ATTACHMENTS];
    uint32_t mNumColorAttachments = 0;
    uint32_t mClearAttachments = 0;
    uint32_t mLoadAttachments = 0;
    uint32_t mStoreAttachments = 0;
};

struct RenderPass
{
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
};
