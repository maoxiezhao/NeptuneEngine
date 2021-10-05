#pragma once

#include "common.h"
#include "utils\objectPool.h"
#include "utils\intrusivePtr.hpp"
#include "utils\log.h"
#include "utils\hash.h"
#include "vulkanCache.h"

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

namespace GPU
{

static const U32 VULKAN_NUM_ATTACHMENTS = 8;
static const U32 VULKAN_NUM_DESCRIPTOR_SETS = 4;
static const U32 VULKAN_NUM_VERTEX_ATTRIBS = 16;
static const U32 VULKAN_NUM_VERTEX_BUFFERS = 8;
static const U32 VULKAN_NUM_BINDINGS = 32;
static const U32 VULKAN_PUSH_CONSTANT_SIZE = 128;
static const U32 VULKAN_NUM_BINDINGS_BINDLESS_VARYING = 16 * 1024;

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
    Image* image = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

struct ImageCreateInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t levels = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageType type = VK_IMAGE_TYPE_2D;
    uint32_t layers = 1;
    VkImageUsageFlags usage = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageCreateFlags flags = 0;
    uint32_t misc;
    VkImageLayout initialLayout = VK_IMAGE_LAYOUT_GENERAL;

    static ImageCreateInfo renderTarget(uint32_t width, uint32_t height, VkFormat format)
    {
        ImageCreateInfo info = {};
        info.width = width;
        info.height = height;
        info.format = format;
        info.usage = (IsFormatHasDepth(format) || IsFormatHasStencil(format) ?
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        info.initialLayout = IsFormatHasDepth(format) || IsFormatHasStencil(format) ?
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        return info;
    }
};

class GraphicsCookie
{
public:
    GraphicsCookie(uint64_t cookie) : cookie(cookie) {}

    uint64_t GetCookie()const
    {
        return cookie;
    }

private:
    uint64_t cookie;
};

struct VertexAttribState
{
    uint32_t binding;
    VkFormat format;
    uint32_t offset;
};

struct BlendState
{
    bool alphaToCoverageEnable = false;
    bool independentBlendEnable = false;
    uint8_t numRenderTarget = 1;

    struct RenderTargetBlendState
    {
        bool blendEnable = false;
        VkBlendFactor srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
        VkBlendFactor destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkBlendOp blendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcBlendAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendFactor destBlendAlpha = VK_BLEND_FACTOR_ONE;
        VkBlendOp blendOpAlpha = VK_BLEND_OP_ADD;
        uint8_t renderTargetWriteMask = (uint8_t)COLOR_WRITE_ENABLE_ALL;
    };
    RenderTargetBlendState renderTarget[8];
};

struct RasterizerState
{
    FillMode fillMode = FILL_SOLID;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    bool frontCounterClockwise = false;
    int32_t depthBias = 0;
    float depthBiasClamp = 0.0f;
    float slopeScaledDepthBias = 0.0f;
    bool depthClipEnable = false;
    bool multisampleEnable = false;
    bool antialiasedLineEnable = false;
    bool conservativeRasterizationEnable = false;
    uint32_t forcedSampleCount = 0;
};

struct DepthStencilState
{
    bool depthEnable = false;
    DepthWriteMask depthWriteMask = DEPTH_WRITE_MASK_ZERO;
    VkCompareOp depthFunc = VK_COMPARE_OP_NEVER;
    bool stencilEnable = false;
    uint8_t stencilReadMask = 0xff;
    uint8_t stencilWriteMask = 0xff;

    struct DepthStencilOp
    {
        VkStencilOp stencilFailOp = VK_STENCIL_OP_KEEP;
        VkStencilOp stencilDepthFailOp = VK_STENCIL_OP_KEEP;
        VkStencilOp stencilPassOp = VK_STENCIL_OP_KEEP;
        VkCompareOp stencilFunc = VK_COMPARE_OP_NEVER;
    };
    DepthStencilOp frontFace;
    DepthStencilOp backFace;
};

}