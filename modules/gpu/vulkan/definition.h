#pragma once

#include "core\common.h"
#include "core\utils\objectPool.h"
#include "core\utils\intrusivePtr.hpp"
#include "core\utils\log.h"
#include "core\utils\stackAllocator.h"
#include "core\utils\tempHashMap.h"
#include "core\utils\intrusiveHashMap.hpp"
#include "math\hash.h"

// #include "vulkanCache.h"

#define VULKAN_TEST_FILESYSTEM

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

#define VULKAN_DEBUG
#define VULKAN_MT

//fossilize
#define VULKAN_TEST_FOSSILIZE

namespace VulkanTest
{
namespace GPU
{
#ifdef VULKAN_MT
    template<typename T>
    using ObjectPool = Util::ThreadSafeObjectPool<T>;

    template<typename T>
    using VulkanCache = Util::ThreadSafeIntrusiveHashMapReadCached<T>;

    template <typename T, typename Deleter = std::default_delete<T>>
    using IntrusivePtrEnabled = Util::IntrusivePtrEnabled<T, Deleter, Util::MultiThreadCounter>;
#else
    template<typename T>
    using ObjectPool = Util::ObjectPool;

    template<typename T>
    using VulkanCache = Util::IntrusiveHashMap<T>;

    template <typename T, typename Deleter = std::default_delete<T>>
    using IntrusivePtrEnabled = Util::IntrusivePtrEnabled<T, Deleter, Util::SingleThreadCounter>;
#endif

    template<typename T>
    using IntrusivePtr = Util::IntrusivePtr<T>;

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
        QUEUE_INDEX_COUNT
    };

    enum QueueType
    {
        QUEUE_TYPE_GRAPHICS = QUEUE_INDEX_GRAPHICS,
        QUEUE_TYPE_ASYNC_COMPUTE,
        QUEUE_TYPE_ASYNC_TRANSFER,
        QUEUE_TYPE_COUNT,
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

    enum InputClassification
    {
        PER_VERTEX_DATA,
        PER_INSTANCE_DATA,
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

    static inline bool IsFormatHasDepthOrStencil(VkFormat format)
    {
        return IsFormatHasDepth(format) || IsFormatHasStencil(format);
    }

    static inline VkImageAspectFlags formatToAspectMask(VkFormat format)
    {
        switch (format)
        {
        case VK_FORMAT_UNDEFINED:
            return 0;

        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;

        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT;

        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;

        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
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

    enum class ImageDomain
    {
        Physical,
        Transient,
        LinearHostCached,
        LinearHost
    };

    enum class BufferDomain
    {
        Device,             // Device local. Probably not visible from CPU.
        LinkedDeviceHost,   // On desktop, directly mapped VRAM over PCI.
        Host,               // Host-only, needs to be synced to GPU. Might be device local as well on iGPUs.
        CachedHost,
    };

    enum ImageViewMiscFlagBits
    {
        IMAGE_VIEW_MISC_FORCE_ARRAY_BIT = 1 << 0
    };

    class Image;
    struct ImageViewCreateInfo
    {
        Image* image = nullptr;
        VkFormat format = VK_FORMAT_UNDEFINED;
        U32 baseLevel = 0;
        U32 levels = VK_REMAINING_MIP_LEVELS;
        U32 baseLayer = 0;
        U32 layers = VK_REMAINING_ARRAY_LAYERS;
        U32 misc = 0;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        VkComponentMapping swizzle = {
            VK_COMPONENT_SWIZZLE_R, 
            VK_COMPONENT_SWIZZLE_G, 
            VK_COMPONENT_SWIZZLE_B, 
            VK_COMPONENT_SWIZZLE_A,
        };
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
        ImageDomain domain = ImageDomain::Physical;

        static ImageCreateInfo renderTarget(uint32_t width, uint32_t height, VkFormat format)
        {
            ImageCreateInfo info = {};
            info.width = width;
            info.height = height;
            info.format = format;
            info.type = VK_IMAGE_TYPE_2D;
            info.usage = (IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.initialLayout = IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            return info;
        }

        static ImageCreateInfo TransientRenderTarget(uint32_t width, uint32_t height, VkFormat format)
        {
            ImageCreateInfo info = {};
            info.width = width;
            info.height = height;
            info.format = format;
            info.type = VK_IMAGE_TYPE_2D;
            info.usage = (IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.initialLayout = IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            return info;
        }

        static ImageCreateInfo ImmutableImage2D(U32 width, U32 height, VkFormat format)
        {
            ImageCreateInfo info = {};
            info.width = width;
            info.height = height;
            info.format = format;
            info.type = VK_IMAGE_TYPE_2D;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return info;
        }
    };

    class InternalSyncObject
    {
    public:
        void SetInternalSyncObject()
        {
            internalSync = true;
        }

    protected:
        bool internalSync = false;
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

    struct SubresourceData
    {
        const void *data = nullptr;
        U32 rowPitch = 0;
        U32 slicePitch = 0;
    };

    enum BufferMiscFlagBits
    {
        BUFFER_MISC_ZERO_INITIALIZE_BIT = 1 << 0
    };
    struct BufferCreateInfo
    {
        BufferDomain domain = BufferDomain::Device;
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        U32 misc = 0;
    };

    class Buffer;
    struct BufferViewCreateInfo
    {
        const Buffer* buffer;
        VkFormat format;
        VkDeviceSize offset;
        VkDeviceSize range;
    };

    struct SamplerCreateInfo
    {
        VkFilter magFilter;
        VkFilter minFilter;
        VkSamplerMipmapMode mipmapMode;
        VkSamplerAddressMode addressModeU;
        VkSamplerAddressMode addressModeV;
        VkSamplerAddressMode addressModeW;
        float mipLodBias;
        VkBool32 anisotropyEnable;
        float maxAnisotropy;
        VkBool32 compareEnable;
        VkCompareOp compareOp;
        float minLod;
        float maxLod;
        VkBorderColor borderColor;
        VkBool32 unnormalizedCoordinates;
    };
}
}