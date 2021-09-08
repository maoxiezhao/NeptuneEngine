#pragma once

#include "common.h"
#include "utils\objectPool.h"
#include "utils\intrusivePtr.hpp"

// platform win32
#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>

#define VK_USE_PLATFORM_WIN32_KHR
#endif

// vulkan
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#include "GLFW\glfw3.h"

static const uint32_t VULKAN_NUM_ATTACHMENTS = 8;

class DeviceVulkan;

enum class ShaderStage
{
    MS = 0,
    AS,
    VS,
    HS,
    DS,
    GS,
    PS,
    CS,
    LIB,
    Count
};

enum QueueIndices
{
    QUEUE_INDEX_GRAPHICS,
    QUEUE_INDEX_COMPUTE,
    QUEUE_INDEX_TRANSFER,
    QUEUE_INDEX_VIDEO_DECODE,
    QUEUE_INDEX_COUNT
};

enum class QueueType
{
    GRAPHICS = QUEUE_INDEX_GRAPHICS,
    Count
};

enum class SwapchainRenderPassType
{
    ColorOnly,
    Depth,
    DepthStencil
};

static inline bool IsFormatHasDepth(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

static inline bool IsFormatHasStencil(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT:
    case VK_FORMAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

enum class DepthStencilMode
{
    None,
    ReadOnly,
    ReadWrite
};

struct PipelineLayout
{
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
};

struct Shader
{
    ShaderStage mShaderStage;
    VkShaderModule mShaderModule = VK_NULL_HANDLE;
};

struct ShaderProgram
{
public:
    PipelineLayout* GetPipelineLayout()const
    {
        return mPipelineLayout;
    }

    inline const Shader* GetShader(ShaderStage stage) const
    {
        return mShaders[UINT(stage)];
    }

private:
    void SetShader(ShaderStage stage, Shader* handle);

    DeviceVulkan* mDevice;
    PipelineLayout* mPipelineLayout = nullptr;;
    Shader* mShaders[UINT(ShaderStage::Count)] = {};
};

struct PipelineStateDesc
{
    ShaderProgram mShaderProgram;
};

class Image;
struct ImageViewCreateInfo
{
    Image* mImage = nullptr;
    VkFormat mFormat = VK_FORMAT_UNDEFINED;
};

struct ImageCreateInfo
{
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    uint32_t mDepth = 1;
    uint32_t mLevels = 1;
    VkFormat mFormat = VK_FORMAT_UNDEFINED;
    VkImageType mType = VK_IMAGE_TYPE_2D;
    uint32_t mLayers = 1;
    VkImageUsageFlags mUsage = 0;
    VkSampleCountFlagBits mSamples = VK_SAMPLE_COUNT_1_BIT;
    VkImageCreateFlags mFlags = 0;
    uint32_t mMisc;
    VkImageLayout mInitialLayout = VK_IMAGE_LAYOUT_GENERAL;

    static ImageCreateInfo RenderTarget(uint32_t width, uint32_t height, VkFormat format)
    {
        ImageCreateInfo info = {};
        info.mWidth = width;
        info.mHeight = height;
        info.mFormat = format;
        info.mUsage = (IsFormatHasDepth(format) || IsFormatHasStencil(format) ?
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        info.mInitialLayout = IsFormatHasDepth(format) || IsFormatHasStencil(format) ?
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        return info;
    }
};

class GraphicsCookie
{
public:
    GraphicsCookie(uint64_t cookie) : mCookie(cookie) {}

    uint64_t GetCookie()const
    {
        return mCookie;
    }

private:
    uint64_t mCookie;
};