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
constexpr unsigned VULKAN_NUM_VERTEX_ATTRIBS = 16;
constexpr unsigned VULKAN_NUM_VERTEX_BUFFERS = 8;

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

enum QueueType
{
    QUEUE_TYPE_GRAPHICS = QUEUE_INDEX_GRAPHICS,
    QUEUE_TYPE_COUNT,
};

enum class SwapchainRenderPassType
{
    ColorOnly,
    Depth,
    DepthStencil
};

enum ColorWriteMask
{
    COLOR_WRITE_DISABLE = 0,
    COLOR_WRITE_ENABLE_RED = 1 << 0,
    COLOR_WRITE_ENABLE_GREEN = 1 << 1,
    COLOR_WRITE_ENABLE_BLUE = 1 << 2,
    COLOR_WRITE_ENABLE_ALPHA = 1 << 3,
    COLOR_WRITE_ENABLE_ALL = ~0,
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

enum FillMode
{
    FILL_WIREFRAME,
    FILL_SOLID,
};

enum DepthWriteMask
{
    DEPTH_WRITE_MASK_ZERO,
    DEPTH_WRITE_MASK_ALL,
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

struct VertexAttribState
{
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

struct BlendState
{
    bool AlphaToCoverageEnable = false;
    bool IndependentBlendEnable = false;
    uint8_t NumRenderTarget = 1;

    struct RenderTargetBlendState
    {
        bool BlendEnable = false;
        VkBlendFactor SrcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor DestBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkBlendOp BlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor SrcBlendAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendFactor DestBlendAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendOp BlendOpAlpha = VK_BLEND_OP_ADD;
        uint8_t RenderTargetWriteMask = (uint8_t)COLOR_WRITE_ENABLE_ALL;
    };
    RenderTargetBlendState RenderTarget[8];
};

struct RasterizerState
{
    FillMode FillMode = FILL_SOLID;
    VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
    bool FrontCounterClockwise = false;
    int32_t DepthBias = 0;
    float DepthBiasClamp = 0.0f;
    float SlopeScaledDepthBias = 0.0f;
    bool DepthClipEnable = false;
    bool MultisampleEnable = false;
    bool AntialiasedLineEnable = false;
    bool ConservativeRasterizationEnable = false;
    uint32_t ForcedSampleCount = 0;
};

struct DepthStencilState
{
    bool DepthEnable = false;
    DepthWriteMask DepthWriteMask = DEPTH_WRITE_MASK_ZERO;
    VkCompareOp DepthFunc = VK_COMPARE_OP_NEVER;
    bool StencilEnable = false;
    uint8_t StencilReadMask = 0xff;
    uint8_t StencilWriteMask = 0xff;

    struct DepthStencilOp
    {
        VkStencilOp StencilFailOp = VK_STENCIL_OP_KEEP;
        VkStencilOp StencilDepthFailOp = VK_STENCIL_OP_KEEP;
        VkStencilOp StencilPassOp = VK_STENCIL_OP_KEEP;
        VkCompareOp StencilFunc = VK_COMPARE_OP_NEVER;
    };
    DepthStencilOp FrontFace;
    DepthStencilOp BackFace;
};