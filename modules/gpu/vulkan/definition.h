#pragma once

#include "core\common.h"
#include "core\utils\objectPool.h"
#include "core\utils\intrusivePtr.hpp"
#include "core\utils\log.h"
#include "core\utils\stackAllocator.h"
#include "core\utils\tempHashMap.h"
#include "core\collections\intrusiveHashMap.hpp"
#include "core\collections\array.h"
#include "math\hash.h"

// #include "vulkanCache.h"

#define VULKAN_TEST_FILESYSTEM

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif // _WIN32

// platform win32
#ifdef CJING3D_PLATFORM_WIN32
#include <SDKDDKVer.h>
#include <windows.h>
#include <tchar.h>
#endif

// vulkan
#define VK_NO_PROTOTYPES
#include "vulkan\vulkan.h"
#include "volk\volk.h"

#define VULKAN_DEBUG
#define VULKAN_MT

//fossilize
#define VULKAN_TEST_FOSSILIZE

// Binding shift. See shader compiler
#define VULKAN_BINDING_SHIFT_B 0
#define VULKAN_BINDING_SHIFT_T 1000
#define VULKAN_BINDING_SHIFT_U 2000
#define VULKAN_BINDING_SHIFT_S 3000

namespace VulkanTest
{
namespace GPU
{
#ifdef VULKAN_MT
    template<typename T>
    using ObjectPool = Util::ThreadSafeObjectPool<T>;

    template<typename T>
    using VulkanCache = Util::ThreadSafeIntrusiveHashMapReadCached<T>;

    template <typename T>
    using VulkanCacheReadWrite = Util::ThreadSafeIntrusiveHashMap<T>;

    template <typename T, typename Deleter = std::default_delete<T>>
    using IntrusivePtrEnabled = Util::IntrusivePtrEnabled<T, Deleter, Util::MultiThreadCounter>;
#else
    template<typename T>
    using ObjectPool = Util::ObjectPool;

    template<typename T>
    using VulkanCache = Util::IntrusiveHashMap<T>;

    template <typename T>
    using VulkanCacheReadWrite = Util::IntrusiveHashMap<T>;

    template <typename T, typename Deleter = std::default_delete<T>>
    using IntrusivePtrEnabled = Util::IntrusivePtrEnabled<T, Deleter, Util::SingleThreadCounter>;
#endif

    static const U32 BUFFERCOUNT = 2;
    static constexpr U32 GetBufferCount() { return BUFFERCOUNT; }

    template<typename T>
    using IntrusivePtr = Util::IntrusivePtr<T>;

    static const U32 VULKAN_NUM_ATTACHMENTS = 8;
    static const U32 VULKAN_NUM_DESCRIPTOR_SETS = 4;
    static const U32 VULKAN_NUM_BINDLESS_DESCRIPTOR_SETS = 16;
    static const U32 VULKAN_NUM_VERTEX_ATTRIBS = 16;
    static const U32 VULKAN_NUM_VERTEX_BUFFERS = 8;
    static const U32 VULKAN_NUM_BINDINGS = 16;
    static const U32 VULKAN_PUSH_CONSTANT_SIZE = 128;
    static const U32 VULKAN_MAX_UBO_SIZE = 16 * 1024;
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

    enum DescriptorSetType
    {
        DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE,
        DESCRIPTOR_SET_TYPE_STORAGE_IMAGE,
        DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER,
        DESCRIPTOR_SET_TYPE_STORAGE_BUFFER,
        DESCRIPTOR_SET_TYPE_UNIFORM_TEXEL_BUFFER,
        DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT,
        DESCRIPTOR_SET_TYPE_SAMPLER,
        DESCRIPTOR_SET_TYPE_COUNT,
    };

    enum BindingResourceType
    {
        BINDING_RESOURCE_TYPE_UNDEFINED = 0,
        BINDING_RESOURCE_TYPE_SAMPLER = 1 << 0,
        BINDING_RESOURCE_TYPE_CBV = 1 << 1,
        BINDING_RESOURCE_TYPE_SRV = 1 << 2,
        BINDING_RESOURCE_TYPE_UAV = 1 << 3,
    };

    enum class IndexBufferFormat
    {
        UINT16,
        UINT32,
    };

    static inline U32 GetRolledBinding(U32 unrolledBinding)
    {
        return (unrolledBinding % VULKAN_BINDING_SHIFT_T);
    }

    static inline U32 GetUnrolledBinding(U32 binding, BindingResourceType type)
    {
        switch (type)
        {
        case BINDING_RESOURCE_TYPE_SRV:
            binding += VULKAN_BINDING_SHIFT_T;
            break;
        case BINDING_RESOURCE_TYPE_UAV:
            binding += VULKAN_BINDING_SHIFT_U;
            break;
        case BINDING_RESOURCE_TYPE_CBV:
            binding += VULKAN_BINDING_SHIFT_B;
            break;
        case BINDING_RESOURCE_TYPE_SAMPLER:
            binding += VULKAN_BINDING_SHIFT_S;
            break;
        default:
            ASSERT(false);
            break;
        }
        return binding;
    }

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
        Host,               // Host-only, needs to be synced to GPU. Might be device local as well on iGPUs. Like staging buffer.
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
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
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

        static ImageCreateInfo RenderTarget(uint32_t width, uint32_t height, VkFormat format)
        {
            ImageCreateInfo info = {};
            info.width = width;
            info.height = height;
            info.format = format;
            info.type = VK_IMAGE_TYPE_2D;
            info.usage = (IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.initialLayout = IsFormatHasDepth(format) || IsFormatHasStencil(format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            return info;
        }

        static ImageCreateInfo TransientRenderTarget(uint32_t width, uint32_t height, VkFormat format)
        {
            ImageCreateInfo info = {};
            info.domain = ImageDomain::Transient;
            info.width = width;
            info.height = height;
            info.format = format;
            info.type = VK_IMAGE_TYPE_2D;
            info.usage = (IsFormatHasDepthOrStencil(format) ? 
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.flags = 0;
            info.misc = 0;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

    enum StencilFace
    {
        STENCIL_FACE_FRONT,
        STENCIL_FACE_BACK,
        STENCIL_FACE_FRONT_AND_BACK
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
        U32 rowLength = 0;
        U32 imageHeight = 0;
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
        VkBool32 anisotropyEnable = false;
        float maxAnisotropy = 1.0f;
        VkBool32 compareEnable = false;
        VkCompareOp compareOp;
        float minLod = 0;
        float maxLod = VK_LOD_CLAMP_NONE;
        VkBorderColor borderColor;
        VkBool32 unnormalizedCoordinates;
    };

    struct InputLayout
    {
        enum { MAX_ATTRIBUTES = 16 };
        static const uint32_t APPEND_ALIGNED_ELEMENT = ~0u;

        struct Attribute
        {
            VkFormat format = VK_FORMAT_UNDEFINED;
            VkDeviceSize offset = 0;
            InputClassification slotClass = InputClassification::PER_VERTEX_DATA;
        };
        Attribute attributes[MAX_ATTRIBUTES];
        U8 attributeCount = 0;

        void AddAttribute(VkFormat format, VkDeviceSize offset = 0, InputClassification slotClass = InputClassification::PER_VERTEX_DATA)
        {
            if (attributeCount >= LengthOf(attributes)) 
            {
                ASSERT(false);
                return;
            }

            Attribute& attr = attributes[attributeCount];
            attr.format = format;
            attr.offset = offset;
            attr.slotClass = slotClass;
            ++attributeCount;
        }
    };

    struct Viewport
    {
        float x = 0;
        float y = 0;
        float width = 0;
        float height = 0;
        float minDepth = 0;
        float maxDepth = 1;
    };

}
}