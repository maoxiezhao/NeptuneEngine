#include "device.h"
#include "core\utils\hash.h"
#include "spriv_reflect\spirv_reflect.h"
#include "memory.h"
#include "TextureFormatLayout.h"

namespace GPU
{
namespace 
{
	static const QueueIndices QUEUE_FLUSH_ORDER[] = {
		QUEUE_INDEX_GRAPHICS,
		QUEUE_INDEX_COMPUTE
	};

    QueueIndices ConvertQueueTypeToIndices(QueueType type)
    {
        return static_cast<QueueIndices>(type);
    }

    static uint64_t STATIC_COOKIE = 0;

    static inline VkImageViewType GetImageViewType(const ImageCreateInfo& createInfo, const ImageViewCreateInfo* view)
    {
        unsigned layers = view ? view->layers : createInfo.layers;
        unsigned base_layer = view ? view->baseLayer : 0;

        if (layers == VK_REMAINING_ARRAY_LAYERS)
            layers = createInfo.layers - base_layer;

        bool force_array =
            view ? (view->misc & IMAGE_VIEW_MISC_FORCE_ARRAY_BIT) : (createInfo.misc & IMAGE_MISC_FORCE_ARRAY_BIT);

        switch (createInfo.type)
        {
        case VK_IMAGE_TYPE_1D:
            assert(createInfo.width >= 1);
            assert(createInfo.height == 1);
            assert(createInfo.depth == 1);
            assert(createInfo.samples == VK_SAMPLE_COUNT_1_BIT);

            if (layers > 1 || force_array)
                return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
            else
                return VK_IMAGE_VIEW_TYPE_1D;

        case VK_IMAGE_TYPE_2D:
            assert(createInfo.width >= 1);
            assert(createInfo.height >= 1);
            assert(createInfo.depth == 1);

            if ((createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) && (layers % 6) == 0)
            {
                assert(createInfo.width == createInfo.height);

                if (layers > 6 || force_array)
                    return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
                else
                    return VK_IMAGE_VIEW_TYPE_CUBE;
            }
            else
            {
                if (layers > 1 || force_array)
                    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                else
                    return VK_IMAGE_VIEW_TYPE_2D;
            }

        case VK_IMAGE_TYPE_3D:
            assert(createInfo.width >= 1);
            assert(createInfo.height >= 1);
            assert(createInfo.depth >= 1);
            return VK_IMAGE_VIEW_TYPE_3D;

        default:
            assert(0 && "bogus");
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        }
    }

    static QueueIndices GetQueueIndexFromQueueType(QueueType queueType)
    {
        return QueueIndices(queueType);
    }

    class ImageResourceCreator
    {
    public:
        DeviceVulkan& device;
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkImageView depthView = VK_NULL_HANDLE;
        VkImageView stencilView = VK_NULL_HANDLE;
        std::vector<VkImageView> rtViews;
        VkImageViewCreateInfo defaultViewCreateInfo = {};
        bool imageOwned = false;

    public:
        explicit ImageResourceCreator(DeviceVulkan& device_) :
            device(device_)
        {
        }

        explicit ImageResourceCreator(DeviceVulkan& device_, VkImage image_) :
            device(device_),
            image(image_),
            imageOwned(false)
        {
        }

        bool CreateDefaultView(const VkImageViewCreateInfo& viewInfo)
        {
            return vkCreateImageView(device.device, &viewInfo, nullptr, &imageView) == VK_SUCCESS;
        }

        bool CreatDepthAndStencilView(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo& viewInfo)
        {
            if (viewInfo.subresourceRange.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            {
                if ((createInfo.usage & ~VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
                {
                    auto tempInfo = viewInfo;
                    tempInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &depthView) != VK_SUCCESS)
                        return false;

                    tempInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &stencilView) != VK_SUCCESS)
                        return false;
                }
            }
            return true;
        }

        bool CreateRenderTargetView(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo& viewInfo)
        {
            if (viewInfo.subresourceRange.layerCount > 0 && createInfo.usage & (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
            {
                rtViews.reserve(viewInfo.subresourceRange.layerCount);

                VkImageViewCreateInfo tempInfo = viewInfo;
                tempInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

                for (U32 layer = 0; layer < tempInfo.subresourceRange.layerCount; layer++)
                {
                    tempInfo.subresourceRange.levelCount = 1;
                    tempInfo.subresourceRange.layerCount = 1;
                    tempInfo.subresourceRange.baseArrayLayer = viewInfo.subresourceRange.baseArrayLayer + layer;

                    VkImageView rtView = VK_NULL_HANDLE;
                    if (vkCreateImageView(device.device, &tempInfo, nullptr, &rtView) != VK_SUCCESS)
                        return false;

                    rtViews.push_back(rtView);
                }
            }

            return true;
        }

        void CreateDefaultViewCreateInfo(const ImageCreateInfo& imageCreateInfo)
        {
            defaultViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            defaultViewCreateInfo.image = image;
            defaultViewCreateInfo.viewType = GetImageViewType(imageCreateInfo, nullptr);
            defaultViewCreateInfo.format = imageCreateInfo.format;
            defaultViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            defaultViewCreateInfo.subresourceRange.aspectMask = formatToAspectMask(imageCreateInfo.format);
            defaultViewCreateInfo.subresourceRange.baseMipLevel = 0;
            defaultViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.levels;
            defaultViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            defaultViewCreateInfo.subresourceRange.layerCount = imageCreateInfo.layers;

        }

        bool CreateDefaultViews(const ImageCreateInfo& createInfo, const VkImageViewCreateInfo* viewInfo)
        {
            U32 checkUsage = 
                VK_IMAGE_USAGE_SAMPLED_BIT | 
                VK_IMAGE_USAGE_STORAGE_BIT | 
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            if ((createInfo.usage & checkUsage) == 0)
            {
                Logger::Error("Cannot create image view unless certain usage flags are present.\n");
                return false;
            }
            if (viewInfo != nullptr)
                defaultViewCreateInfo = *viewInfo;
            else
                CreateDefaultViewCreateInfo(createInfo);
            
            if (!CreatDepthAndStencilView(createInfo, defaultViewCreateInfo))
                return false;

            if (!CreateRenderTargetView(createInfo, defaultViewCreateInfo))
                return false;

            if (!CreateDefaultView(defaultViewCreateInfo))
                return false;

            return true;
        }

        VkImageViewType GetDefaultImageViewType()const
        {
            return defaultViewCreateInfo.viewType;
        }
    };
}

DeviceVulkan::DeviceVulkan() : 
    shaderManager(*this),
    transientAllocator(*this),
    frameBufferAllocator(*this)
{
}

DeviceVulkan::~DeviceVulkan()
{
    WaitIdle();

    wsi.Clear();

    transientAllocator.Clear();
    frameBufferAllocator.Clear();
}

void DeviceVulkan::SetContext(VulkanContext& context)
{
    device = context.device;
    physicalDevice = context.physicalDevice;
    instance = context.instance;
    queueInfo = context.queueInfo;
    features = context.ext;

    // Create frame resources
    InitFrameContext();

    // Init bindless descriptor set allocator
    InitBindless();

    //init managers
    memory.Initialize(this);
    fencePoolManager.Initialize(*this);
    semaphoreManager.Initialize(*this);
}

void DeviceVulkan::InitFrameContext()
{
    transientAllocator.Clear();
    frameBufferAllocator.Clear();
    frameResources.clear();

    const int FRAME_COUNT = 2;
    for (int frameIndex = 0; frameIndex < FRAME_COUNT; frameIndex++)
    {
        auto frameResource = std::make_unique<FrameResource>(*this);
        frameResources.emplace_back(std::move(frameResource));
    }
}

bool DeviceVulkan::InitSwapchain(std::vector<VkImage>& images, VkFormat format, uint32_t width, uint32_t height)
{
    wsi.swapchainImages.clear();

    ImageCreateInfo imageCreateInfo = ImageCreateInfo::renderTarget(width, height, format);
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    for (int i = 0; i < images.size(); i++)
    {
        VkImageView imageView;
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(device, &createInfo, nullptr, &imageView);
        assert(res == VK_SUCCESS);

        ImagePtr backbuffer = ImagePtr(imagePool.allocate(*this, images[i], imageView, DeviceAllocation(), imageCreateInfo));
        if (backbuffer)
        {
            backbuffer->DisownImge();
            backbuffer->SetSwapchainLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            wsi.swapchainImages.push_back(backbuffer);
            SetName(*backbuffer.get(), "Backbuffer");
        }
    }

    return true;
}

Util::IntrusivePtr<CommandList> DeviceVulkan::RequestCommandList(QueueType queueType)
{
    return RequestCommandList(0, queueType);
}

Util::IntrusivePtr<CommandList> DeviceVulkan::RequestCommandList(int threadIndex, QueueType queueType)
{
    // Only support main thread now
    auto& pools = CurrentFrameResource().cmdPools[(int)queueType];
    assert(threadIndex == 0 && threadIndex < pools.size());

    CommandPool& pool = pools[threadIndex];
    VkCommandBuffer buffer = pool.RequestCommandBuffer();
    if (buffer == VK_NULL_HANDLE)
        return Util::IntrusivePtr<CommandList>();

    VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult res = vkBeginCommandBuffer(buffer, &info);
    assert(res == VK_SUCCESS);

    return Util::IntrusivePtr<CommandList>(commandListPool.allocate(*this, buffer, queueType));
}

ImageView& DeviceVulkan::GetSwapchainView()
{
    assert(wsi.swapchainImages.size() > 0);
    return wsi.swapchainImages[wsi.index]->GetImageView();
}

RenderPassInfo DeviceVulkan::GetSwapchianRenderPassInfo(SwapchainRenderPassType swapchainRenderPassType)
{
    RenderPassInfo info = {};
    info.numColorAttachments = 1;
    info.colorAttachments[0] = &GetSwapchainView();
    info.clearAttachments = ~0u;
    info.storeAttachments = 1u << 0;

    if (swapchainRenderPassType == SwapchainRenderPassType::Depth)
    {
        info.opFlags |= RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
        info.depthStencil;
    }

    return info;
}

RenderPass& DeviceVulkan::RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isCompatible)
{
    HashCombiner hash;
    VkFormat colorFormats[VULKAN_NUM_ATTACHMENTS];
    VkFormat depthStencilFormat = VkFormat::VK_FORMAT_UNDEFINED;

    // Color attachments
    for (uint32_t i = 0; i < renderPassInfo.numColorAttachments; i++)
    {
        const ImageView& imageView = *renderPassInfo.colorAttachments[i];
        colorFormats[i] = imageView.GetInfo().format;
        hash.HashCombine(imageView.GetImage()->GetSwapchainLayout());
    }

    // Depth stencil
    if (renderPassInfo.depthStencil)
        depthStencilFormat = renderPassInfo.depthStencil->GetInfo().format;

    // Subpasses
    hash.HashCombine(renderPassInfo.numSubPasses);
    for (uint32_t i = 0; i < renderPassInfo.numSubPasses; i++)
    {
        auto& subpassInfo = renderPassInfo.subPasses[i];
        hash.HashCombine(subpassInfo.numColorAattachments);
        hash.HashCombine(subpassInfo.numInputAttachments);
        hash.HashCombine(subpassInfo.numResolveAttachments);
		for (uint32_t j = 0; j < subpassInfo.numColorAattachments; j++)
            hash.HashCombine(subpassInfo.colorAttachments[j]);
		for (uint32_t j = 0; j < subpassInfo.numInputAttachments; j++)
            hash.HashCombine(subpassInfo.inputAttachments[j]);
		for (uint32_t j = 0; j < subpassInfo.numResolveAttachments; j++)
            hash.HashCombine(subpassInfo.resolveAttachments[j]);
    }

    // Calculate hash
	for (uint32_t i = 0; i < renderPassInfo.numColorAttachments; i++)
        hash.HashCombine((uint32_t)colorFormats[i]);

    hash.HashCombine(renderPassInfo.numColorAttachments);
    hash.HashCombine((uint32_t)depthStencilFormat);

    // Compatible render passes do not care about load/store
    if (!isCompatible)
    {
        hash.HashCombine(renderPassInfo.clearAttachments);
        hash.HashCombine(renderPassInfo.loadAttachments);
        hash.HashCombine(renderPassInfo.storeAttachments);
    }

    auto findIt = renderPasses.find(hash.Get());
    if (findIt != renderPasses.end())
        return *findIt->second;

    RenderPass& renderPass = *renderPasses.emplace(hash.Get(), *this, renderPassInfo);
    renderPass.SetHash(hash.Get());
    return renderPass;
}

FrameBuffer& DeviceVulkan::RequestFrameBuffer(const RenderPassInfo& renderPassInfo)
{
    return frameBufferAllocator.RequestFrameBuffer(renderPassInfo);
}

PipelineLayout& DeviceVulkan::RequestPipelineLayout(const CombinedResourceLayout& resLayout)
{
    HashCombiner hash;

    auto findIt = pipelineLayouts.find(hash.Get());
    if (findIt != pipelineLayouts.end())
        return *findIt->second;

    PipelineLayout& pipelineLayout = *pipelineLayouts.emplace(hash.Get(), *this, resLayout);
    pipelineLayout.SetHash(hash.Get());
    return pipelineLayout;
}

SemaphorePtr DeviceVulkan::RequestSemaphore()
{
    VkSemaphore semaphore = semaphoreManager.Requset();
    return SemaphorePtr(semaphorePool.allocate(*this, semaphore, false));
}

Shader& DeviceVulkan::RequestShader(ShaderStage stage, const void* pShaderBytecode, size_t bytecodeLength, const ShaderResourceLayout* layout)
{
    HashCombiner hash;
    hash.HashCombine(static_cast<const uint32_t*>(pShaderBytecode), bytecodeLength);

    auto findIt = shaders.find(hash.Get());
    if (findIt != shaders.end())
        return *findIt->second;

    Shader& shader = *shaders.emplace(hash.Get(), *this, stage, pShaderBytecode, bytecodeLength, layout);
    shader.SetHash(hash.Get());
    return shader;
}

void DeviceVulkan::SetName(const Image& image, const char* name)
{
    if (features.supportDebugUtils)
    {
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_IMAGE;
        info.objectHandle = (uint64_t)image.GetImage();

        if (vkSetDebugUtilsObjectNameEXT)
            vkSetDebugUtilsObjectNameEXT(device, &info);
    }
}

void DeviceVulkan::SetName(const Buffer& buffer, const char* name)
{
    if (features.supportDebugUtils)
    {
        VkDebugUtilsObjectNameInfoEXT info{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        info.pObjectName = name;
        info.objectType = VK_OBJECT_TYPE_BUFFER;
        info.objectHandle = (uint64_t)buffer.GetBuffer();

        if (vkSetDebugUtilsObjectNameEXT)
            vkSetDebugUtilsObjectNameEXT(device, &info);
    }
}

ShaderProgram* DeviceVulkan::RequestProgram(Shader* shaders[static_cast<U32>(ShaderStage::Count)])
{
    HashCombiner hasher;
    for (int i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
    {
        if (shaders[i] != nullptr)
            hasher.HashCombine(shaders[i]->GetHash());
    }

    auto findIt = programs.find(hasher.Get());
    if (findIt != programs.end())
        return findIt->second;

    ShaderProgramInfo info = {};
    for (int i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
    {
        if (shaders[i] != nullptr)
            info.shaders[i] = shaders[i];
    }

    ShaderProgram* program = programs.emplace(hasher.Get(), this, info);
    program->SetHash(hasher.Get());
    return program;
}

DescriptorSetAllocator& DeviceVulkan::RequestDescriptorSetAllocator(const DescriptorSetLayout& layout, const U32* stageForBinds)
{
	HashCombiner hasher;
    hasher.HashCombine(reinterpret_cast<const uint32_t*>(&layout), sizeof(layout));
    hasher.HashCombine(stageForBinds, sizeof(U32) * VULKAN_NUM_BINDINGS);
	
	auto findIt = descriptorSetAllocators.find(hasher.Get());
	if (findIt != descriptorSetAllocators.end())
		return *findIt->second;

    DescriptorSetAllocator* allocator = descriptorSetAllocators.emplace(hasher.Get(), *this, layout, stageForBinds);
    allocator->SetHash(hasher.Get());
	return *allocator;
}

ImagePtr DeviceVulkan::RequestTransientAttachment(U32 w, U32 h, VkFormat format, U32 index, U32 samples, U32 layers)
{
    return transientAllocator.RequsetAttachment(w, h, format, index, samples, layers);
}

ImagePtr DeviceVulkan::CreateImage(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData)
{
    if (pInitialData != nullptr)
    {
        InitialImageBuffer stagingBuffer = CreateImageStagingBuffer(createInfo, pInitialData);
        return CreateImageFromStagingBuffer(createInfo, &stagingBuffer);
    }   
    else
    {
        return CreateImageFromStagingBuffer(createInfo, nullptr);
    }
}

InitialImageBuffer DeviceVulkan::CreateImageStagingBuffer(const ImageCreateInfo& createInfo, const SubresourceData* pInitialData)
{
    // Setup texture format layout
    TextureFormatLayout layout;
    U32 copyLevels = 1;
    switch (createInfo.type)
    {
    case VK_IMAGE_TYPE_1D:
        layout.SetTexture1D(createInfo.format, createInfo.width, createInfo.layers, copyLevels);
        break;
    case VK_IMAGE_TYPE_2D:
        layout.SetTexture2D(createInfo.format, createInfo.width, createInfo.height, createInfo.layers, copyLevels);
        break;
    case VK_IMAGE_TYPE_3D:
        layout.SetTexture3D(createInfo.format, createInfo.width, createInfo.height, createInfo.depth, copyLevels);
        break;
    default:
        return {};
    }

    // Create staging buffer
    InitialImageBuffer imageBuffer = {};
    BufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.domain = BufferDomain::Host;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.size = layout.GetRequiredSize();
    imageBuffer.buffer = CreateBuffer(bufferCreateInfo, nullptr);
    if (!imageBuffer.buffer)
        return InitialImageBuffer();
    SetName(*imageBuffer.buffer, "image_upload_staging_buffer");

    // Map staging buffer and copy initial datas to staging buffer
    U8* mapped = static_cast<U8*>(MapBuffer(*imageBuffer.buffer, MEMORY_ACCESS_WRITE_BIT));
    if (mapped != nullptr)
    {
        layout.SetBuffer(mapped, layout.GetRequiredSize());

        U32 index = 0;
        U32 blockStride = layout.GetBlockStride();
        for (U32 level = 0; level < copyLevels; level++, index++)
        {
            // Calculate dst stride
            const auto& mipInfo = layout.GetMipInfo(level);
            U32 dstRowSize = mipInfo.blockW * blockStride;
            U32 dstHeightStride = mipInfo.blockH * dstRowSize;

            for (U32 layer = 0; layer < createInfo.layers; layer++)
            {
                U32 srcRowLength = pInitialData[index].rowPitch ? pInitialData[index].rowPitch : mipInfo.rowLength;
                U32 srcArrayHeight = pInitialData[index].slicePitch ? pInitialData[index].slicePitch : mipInfo.imageHeight;
                U32 srcRowStride = (U32)layout.RowByteStride(srcRowLength);
                U32 srcHeightStride = (U32)layout.LayerByteStride(srcArrayHeight, srcRowStride);

                U8* dst = static_cast<uint8_t*>(layout.Data(layer, level));
                const U8* src = static_cast<const uint8_t*>(pInitialData[index].data);

                for (U32 depth = 0; depth < mipInfo.depth; depth++)
                    for (U32 y = 0; y < mipInfo.blockH; y++)
                        memcpy(dst + depth * dstHeightStride + y * dstRowSize, src + depth * srcHeightStride + y * srcRowStride, dstRowSize);
            }
        }
        UnmapBuffer(*imageBuffer.buffer, MEMORY_ACCESS_WRITE_BIT);

        // Create VkBufferImageCopies
        layout.BuildBufferImageCopies(imageBuffer.numBit, imageBuffer.bit);
    }
    return imageBuffer;
}

ImagePtr DeviceVulkan::CreateImageFromStagingBuffer(const ImageCreateInfo& createInfo, const InitialImageBuffer* stagingBuffer)
{
    VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    info.format = createInfo.format;
    info.extent.width = createInfo.width;
    info.extent.height = createInfo.height;
    info.extent.depth = createInfo.depth;
    info.imageType = createInfo.type;
    info.mipLevels = createInfo.levels;
    info.arrayLayers = createInfo.layers;
    info.samples = createInfo.samples;
    info.usage = createInfo.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.flags = createInfo.flags;

    if (createInfo.domain == ImageDomain::Transient)
        info.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    if (stagingBuffer != nullptr)
        info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (createInfo.domain == ImageDomain::LinearHostCached || createInfo.domain == ImageDomain::LinearHost)
    {
        info.tiling = VK_IMAGE_TILING_LINEAR;
        info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    }
    else
    {
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    if (info.tiling == VK_IMAGE_TILING_LINEAR)
    {
        if (stagingBuffer)
            return ImagePtr();
        if (info.mipLevels > 1)
            return ImagePtr();
        if (info.arrayLayers > 1)
            return ImagePtr();
        if (info.imageType != VK_IMAGE_TYPE_2D)
            return ImagePtr();
        if (info.samples != VK_SAMPLE_COUNT_1_BIT)
            return ImagePtr();
    }

    // Check concurrent
    uint32_t queueFlags =createInfo.misc &
        (IMAGE_MISC_CONCURRENT_QUEUE_GRAPHICS_BIT |
        IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_COMPUTE_BIT |
        IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_GRAPHICS_BIT |
        IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_TRANSFER_BIT);
    bool concurrentQueue = queueFlags != 0;

    //VkBuffer buffer;
    VkImage image = VK_NULL_HANDLE;
    DeviceAllocation allocation;
    if (!memory.CreateImage(info, createInfo.domain, image, &allocation))
    {
        Logger::Warning("Failed to create image");
        return ImagePtr(nullptr);
    }
    
    // Create image views
    bool hasView = (info.usage & (
        VK_IMAGE_USAGE_SAMPLED_BIT | 
        VK_IMAGE_USAGE_STORAGE_BIT | 
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)) != 0 &&
        (createInfo.misc & IMAGE_MISC_NO_DEFAULT_VIEWS_BIT) == 0;

    ImageResourceCreator viewCreator(*this, image);
    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    if (hasView)
    {
        if (!viewCreator.CreateDefaultViews(createInfo, nullptr))
            return ImagePtr();

        viewType = viewCreator.GetDefaultImageViewType();
    }

    // Create image ptr
    ImagePtr imagePtr(imagePool.allocate(*this, image, viewCreator.imageView, allocation, createInfo));
    if (!imagePtr)
        return imagePtr;

    if (hasView)
    {
        auto& imageView = imagePtr->GetImageView();
        imageView.SetDepthStencilView(viewCreator.depthView, viewCreator.stencilView);
        imageView.SetRenderTargetViews(std::move(viewCreator.rtViews));
    }

    imagePtr->isOwnsImge = true;
    imagePtr->stageFlags = Image::ConvertUsageToPossibleStages(createInfo.usage);
    imagePtr->accessFlags = Image::ConvertUsageToPossibleAccess(createInfo.usage);

    // If staging buffer is not null, create transition_cmd and copy the staging buffer to gpu buffer
    CommandListPtr transitionCmd;
    if (stagingBuffer != nullptr)
    {
        ASSERT(createInfo.domain != ImageDomain::Transient);
        ASSERT(createInfo.initialLayout != VK_IMAGE_LAYOUT_UNDEFINED);

        VkAccessFlags transitionSrcAccess = 0;
        if (queueInfo.queues[QUEUE_INDEX_GRAPHICS] == queueInfo.queues[QUEUE_INDEX_TRANSFER])
            transitionSrcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;

        CommandListPtr graphicsCmd = RequestCommandList(QueueType::QUEUE_TYPE_GRAPHICS);
        CommandListPtr transferCmd;
        if (queueInfo.queues[QUEUE_INDEX_GRAPHICS] != queueInfo.queues[QUEUE_INDEX_TRANSFER])
            transferCmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_TRANSFER);
        else
            transferCmd = graphicsCmd; 

        // Add image barrier until finish transfer
        transferCmd->ImageBarrier(
            imagePtr, 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            0, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_ACCESS_TRANSFER_WRITE_BIT);

        transferCmd->BeginEvent("Copy_image_to_gpu");
        transferCmd->CopyToImage(imagePtr, stagingBuffer->buffer, stagingBuffer->numBit, stagingBuffer->bit.data());
        transferCmd->EndEvent();

        if (queueInfo.queues[QUEUE_INDEX_GRAPHICS] != queueInfo.queues[QUEUE_INDEX_TRANSFER])
        {
           // If queue is not concurent, we will also need a release + acquire barrier 
           // to marshal ownership from transfer queue over to graphics
            if (!concurrentQueue && queueInfo.familyIndices[QUEUE_INDEX_GRAPHICS] != queueInfo.familyIndices[QUEUE_INDEX_TRANSFER])
            {
                VkImageMemoryBarrier release = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                release.image = imagePtr->GetImage();
                release.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                release.dstAccessMask = 0;
                release.srcQueueFamilyIndex = queueInfo.familyIndices[QUEUE_INDEX_TRANSFER];
                release.dstQueueFamilyIndex = queueInfo.familyIndices[QUEUE_INDEX_GRAPHICS];
                release.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                release.newLayout = createInfo.initialLayout;
                release.subresourceRange.levelCount = info.mipLevels;
                release.subresourceRange.aspectMask = formatToAspectMask(info.format);
                release.subresourceRange.layerCount = info.arrayLayers;

                VkImageMemoryBarrier acquire = release;
                acquire.srcAccessMask = 0;
                acquire.dstAccessMask = imagePtr->GetAccessFlags() & Image::ConvertLayoutToPossibleAccess(createInfo.initialLayout);
            
                transferCmd->Barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 1, &release);
                graphicsCmd->Barrier(imagePtr->GetStageFlags(), imagePtr->GetStageFlags(), 1, &acquire);
            }

            // Submit transfer cmd
            SemaphorePtr semaphore;
            Submit(transferCmd, nullptr, 1, &semaphore);
            AddWaitSemaphore(QueueType::QUEUE_TYPE_GRAPHICS, semaphore, imagePtr->GetStageFlags(), true);
        
            transitionCmd = std::move(graphicsCmd);
        }

        if (transitionCmd)
        {
            Submit(transitionCmd, nullptr, 0, nullptr);
        }
    }
    return imagePtr;
}

ImageViewPtr DeviceVulkan::CreateImageView(const ImageViewCreateInfo& viewInfo)
{
    auto& imageCreateInfo = viewInfo.image->GetCreateInfo();
    VkFormat format = viewInfo.format != VK_FORMAT_UNDEFINED ? viewInfo.format : imageCreateInfo.format;
    VkImageViewCreateInfo createInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    createInfo.image = viewInfo.image->GetImage();
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.components = viewInfo.swizzle;
    createInfo.subresourceRange.aspectMask = formatToAspectMask(format);
    createInfo.subresourceRange.baseMipLevel = viewInfo.baseLevel;
    createInfo.subresourceRange.levelCount = viewInfo.levels;
    createInfo.subresourceRange.baseArrayLayer = viewInfo.baseLayer;
    createInfo.subresourceRange.layerCount = viewInfo.layers;

    if (viewInfo.viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM)
        createInfo.viewType = GetImageViewType(imageCreateInfo, &viewInfo);
    else
        createInfo.viewType = viewInfo.viewType;

    ImageResourceCreator creator(*this);
    if (!creator.CreateDefaultViews(imageCreateInfo, &createInfo))
        return ImageViewPtr();

    ImageViewPtr viewPtr(imageViews.allocate(*this, creator.imageView, viewInfo));
    if (viewPtr)
    {
        return viewPtr;
    }

    return ImageViewPtr();
}

BufferPtr DeviceVulkan::CreateBuffer(const BufferCreateInfo& createInfo, const void* initialData)
{
    VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    info.size = createInfo.size;
    info.usage = createInfo.usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (features.features_1_2.bufferDeviceAddress == VK_TRUE)
    {
        info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    VkBuffer buffer;
    DeviceAllocation allocation;
    if (!memory.CreateBuffer(info, createInfo.domain, buffer, &allocation))
    {
        Logger::Warning("Failed to create buffer");
        return BufferPtr(nullptr);
    }

    auto tmpinfo = createInfo;
    tmpinfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    BufferPtr bufferPtr(buffers.allocate(*this, buffer, allocation, tmpinfo));
    if (!bufferPtr)
    {
        Logger::Warning("Failed to create buffer");
        return BufferPtr(nullptr);
    }
     
    bool needInitialize = (createInfo.misc & BUFFER_MISC_ZERO_INITIALIZE_BIT) != 0 || (initialData != nullptr);
    if (createInfo.domain == BufferDomain::Device && needInitialize && !allocation.IsHostVisible())
    {
        // Buffer is device only, create a staging buffer and copy it
        CommandListPtr cmd;
        if (initialData != nullptr)
        {
            // Create a staging buffer which domain is host
            BufferCreateInfo stagingCreateInfo = createInfo;
            stagingCreateInfo.domain = BufferDomain::Host;
            BufferPtr stagingBuffer = CreateBuffer(stagingCreateInfo, initialData);
            if (!stagingBuffer)
            {
                Logger::Warning("Failed to create buffer");
                return BufferPtr(nullptr);
            }
            SetName(*stagingBuffer, "Buffer_upload_staging_buffer");

            // Request async transfer cmd to copy staging buffer
            cmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_TRANSFER);
            cmd->BeginEvent("Fill_buffer_staging");
            cmd->CopyBuffer(bufferPtr, stagingBuffer);
            cmd->EndEvent();
        }
        else
        {
            cmd = RequestCommandList(QueueType::QUEUE_TYPE_ASYNC_COMPUTE);
            cmd->BeginEvent("Fill_buffer_staging");
            cmd->FillBuffer(bufferPtr, 0);
            cmd->EndEvent();
        }

        if (cmd)
            SubmitStaging(cmd, info.usage, true);
    }
    else if (needInitialize)
    {
        void* ptr = memory.MapMemory(allocation, MemoryAccessFlag::MEMORY_ACCESS_WRITE_BIT, 0, allocation.size);
        if (ptr == nullptr)
            return BufferPtr(nullptr);

        if (initialData != nullptr)
            memcpy(ptr, initialData, createInfo.size);
        else
            memset(ptr, 0, createInfo.size);

        memory.UnmapMemory(allocation, MemoryAccessFlag::MEMORY_ACCESS_WRITE_BIT, 0, allocation.size);
    }
    return bufferPtr;
}

BufferViewPtr DeviceVulkan::CreateBufferView(const BufferViewCreateInfo& viewInfo)
{
    VkBufferViewCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
    info.buffer = viewInfo.buffer->GetBuffer();
    info.format = viewInfo.format;
    info.offset = viewInfo.offset;
    info.range = viewInfo.range;

    VkBufferView view;
    if (vkCreateBufferView(device, &info, nullptr, &view) != VK_SUCCESS)
        return BufferViewPtr();

    return BufferViewPtr(bufferViews.allocate(*this, view, viewInfo));
}

DeviceAllocationOwnerPtr DeviceVulkan::AllocateMemmory(const MemoryAllocateInfo& allocInfo)
{
    DeviceAllocation alloc = {};
    if (!memory.Allocate(
        (U32)allocInfo.requirements.size,
        (U32)allocInfo.requirements.alignment,
        allocInfo.requirements.memoryTypeBits,
        allocInfo.usage,
        &alloc))
        return DeviceAllocationOwnerPtr();

    return DeviceAllocationOwnerPtr(allocations.allocate(*this, alloc));
}

void DeviceVulkan::NextFrameContext()
{
    // submit remain queue
    EndFrameContext();

    transientAllocator.BeginFrame();
    frameBufferAllocator.BeginFrame();

    // descriptor set begin
    for (auto& kvp : descriptorSetAllocators)
        kvp.second->BeginFrame();

    // update frameIndex
    if (++frameIndex >= frameResources.size())
        frameIndex = 0;

    // begin frame resources
    CurrentFrameResource().Begin();
}

void DeviceVulkan::EndFrameContext()
{
    // flush queue
    InternalFence fence;
    auto& frame = CurrentFrameResource();
    auto submissionList = frame.submissions;
    for (auto& queueIndex : QUEUE_FLUSH_ORDER)
    {
        if (queueDatas[queueIndex].needFence || !submissionList[queueIndex].empty())
        {
            SubmitQueue(queueIndex, &fence);
           
            if (fence.fence != VK_NULL_HANDLE)
            {
                frame.waitFences.push_back(fence.fence);
                frame.recyleFences.push_back(fence.fence);
            }
            queueDatas[queueIndex].needFence = false;
        }
    }
}

void DeviceVulkan::FlushFrame(QueueIndices queueIndex)
{
    if (queueInfo.queues[queueIndex] != VK_NULL_HANDLE)
    {
        SubmitQueue(queueIndex, nullptr);
    }
}

void DeviceVulkan::Submit(CommandListPtr& cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore)
{
    SubmitImpl(cmd, fence, semaphoreCount, semaphore);
}

void DeviceVulkan::SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire)
{
    wsi.acquire = std::move(acquire);
    wsi.index = index;
    wsi.consumed = false;
}

void DeviceVulkan::AddWaitSemaphore(QueueType queueType, SemaphorePtr semaphore, VkPipelineStageFlags stages, bool flush)
{
    if (flush)
        FlushFrame(GetQueueIndexFromQueueType(queueType));

    auto& queueData = queueDatas[(int)queueType];
    queueData.waitSemaphores.push_back(semaphore);
    queueData.waitStages.push_back(stages);
    queueData.needFence = true;
}

void DeviceVulkan::ReleaseFrameBuffer(VkFramebuffer buffer)
{
    CurrentFrameResource().destroyedFrameBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseImage(VkImage image)
{
    CurrentFrameResource().destroyedImages.push_back(image);
}

void DeviceVulkan::ReleaseImageView(VkImageView imageView)
{
    CurrentFrameResource().destroyedImageViews.push_back(imageView);
}

void DeviceVulkan::ReleaseFence(VkFence fence, bool isWait)
{
    if (isWait)
    {
        vkResetFences(device, 1, &fence);
        fencePoolManager.Recyle(fence);
    }
    else
    {
        CurrentFrameResource().recyleFences.push_back(fence);
    }
}

void DeviceVulkan::ReleaseBuffer(VkBuffer buffer)
{
    CurrentFrameResource().destroyedBuffers.push_back(buffer);
}

void DeviceVulkan::ReleaseBufferView(VkBufferView bufferView)
{
    CurrentFrameResource().destroyedBufferViews.push_back(bufferView);
}

void DeviceVulkan::ReleasePipeline(VkPipeline pipeline)
{
    CurrentFrameResource().destroyedPipelines.push_back(pipeline);
}

void DeviceVulkan::FreeMemory(const DeviceAllocation& allocation)
{
    CurrentFrameResource().destroyedAllocations.push_back(std::move(allocation));
}

void* DeviceVulkan::MapBuffer(const Buffer& buffer, MemoryAccessFlags flags)
{
    return memory.MapMemory(buffer.allocation, flags, 0, buffer.GetCreateInfo().size);
}

void DeviceVulkan::UnmapBuffer(const Buffer& buffer, MemoryAccessFlags flags)
{
    memory.UnmapMemory(buffer.allocation, flags, 0, buffer.GetCreateInfo().size);
}

void DeviceVulkan::ReleaseSemaphore(VkSemaphore semaphore)
{
    CurrentFrameResource().destroyeSemaphores.push_back(semaphore);
}

void DeviceVulkan::RecycleSemaphore(VkSemaphore semaphore)
{
    CurrentFrameResource().recycledSemaphroes.push_back(semaphore);
}

bool DeviceVulkan::IsImageFormatSupported(VkFormat format, VkFormatFeatureFlags required, VkImageTiling tiling)
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    auto flags = tiling == VK_IMAGE_TILING_OPTIMAL ? props.optimalTilingFeatures : props.linearTilingFeatures;
    return (flags & required) == required;
}

uint64_t DeviceVulkan::GenerateCookie()
{
    STATIC_COOKIE += 16;
    return STATIC_COOKIE;
}

ShaderManager& DeviceVulkan::GetShaderManager()
{
    return shaderManager;
}

SemaphorePtr DeviceVulkan::GetAndConsumeReleaseSemaphore()
{
    auto ret = std::move(wsi.release);
    wsi.release.reset();
    return ret;
}

VkQueue DeviceVulkan::GetPresentQueue() const
{
    return wsi.presentQueue;
}


void DeviceVulkan::BakeShaderProgram(ShaderProgram& program)
{
    // CombinedResourceLayout
    // * descriptorSetMask;
	// * stagesForSets[VULKAN_NUM_DESCRIPTOR_SETS];
	// * stagesForBindings[VULKAN_NUM_DESCRIPTOR_SETS][VULKAN_NUM_BINDINGS];

    // create pipeline layout
    CombinedResourceLayout resLayout;
    resLayout.descriptorSetMask = 0;

    if (program.GetShader(ShaderStage::VS))
        resLayout.attributeInputMask = program.GetShader(ShaderStage::VS)->GetLayout().inputMask;
    if (program.GetShader(ShaderStage::PS))
        resLayout.renderTargetMask = program.GetShader(ShaderStage::VS)->GetLayout().outputMask;

    for (U32 i = 0; i < static_cast<U32>(ShaderStage::Count); i++)
    {
        auto shader = program.GetShader(static_cast<ShaderStage>(i));
        if (shader == nullptr)
            continue;

        U32 stageMask = 1u << i;
        auto& shaderResLayout = shader->GetLayout();
        for (U32 set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
        {
            U32 activeBinds = false;

            // descriptor type masks
            for (U32 maskbit = 0; maskbit < static_cast<U32>(DescriptorSetLayout::SetMask::COUNT); maskbit++)
            {
                resLayout.sets[set].masks[maskbit] |= shaderResLayout.sets[set].masks[maskbit];
                activeBinds |= (resLayout.sets[set].masks[maskbit] != 0);
            }

            // activate current stage
            if (activeBinds != 0)
            {
                resLayout.stagesForSets[set] |= stageMask;

                // activate current bindings
                ForEachBit(activeBinds, [&](uint32_t bit) 
                {
                    resLayout.stagesForBindings[set][bit] |= stageMask; // set->binding->stages

                    auto& combinedSize = resLayout.sets[set].arraySize[bit];
                    auto& shaderSize = shaderResLayout.sets[set].arraySize[bit];
                    if (combinedSize && combinedSize != shaderSize)
                        Logger::Error("Mismatch between array sizes in different shaders.\n");
                    else
                        combinedSize = shaderSize;
                });
            }
        }

        // push constants
        if (shaderResLayout.pushConstantSize > 0)
        {
            resLayout.pushConstantRange.stageFlags |= stageMask;
            resLayout.pushConstantRange.size = std::max(resLayout.pushConstantRange.size, shaderResLayout.pushConstantSize);
        }
    }

    // bindings and check array size
    for (U32 set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
    {
        if (resLayout.stagesForSets[set] != 0)
        {
            resLayout.descriptorSetMask |= 1u << set;

            for(U32 binding = 0; binding < VULKAN_NUM_BINDINGS; binding++)
            {
                uint8_t& arraySize = resLayout.sets[set].arraySize[binding];
                if (arraySize == DescriptorSetLayout::UNSIZED_ARRAY)
                {
					// Allows us to have one unified descriptor set layout for bindless.
					resLayout.stagesForBindings[set][binding] = VK_SHADER_STAGE_ALL;
                }
                else if (arraySize == 0)
                {
                    // just keep one array size
                    arraySize = 1;
                }
            }
        }
    }

    // Create pipeline layout
    HashCombiner hasher;
    hasher.HashCombine(resLayout.pushConstantRange.stageFlags);
    hasher.HashCombine(resLayout.pushConstantRange.size);
    resLayout.pushConstantHash = hasher.Get();
    
    auto& piplineLayout = RequestPipelineLayout(resLayout);
    program.SetPipelineLayout(&piplineLayout);
}

void DeviceVulkan::WaitIdle()
{
    EndFrameContext();

    if (device != VK_NULL_HANDLE)
    {
        auto ret = vkDeviceWaitIdle(device);
        if (ret != VK_SUCCESS)
            Logger::Error("vkDeviceWaitIdle failed:%d", ret);
    }

    frameBufferAllocator.Clear();
    transientAllocator.Clear();

    for (auto& allocator : descriptorSetAllocators)
        allocator.second->Clear();

    for (auto& frame : frameResources)
    {
        frame->waitFences.clear();
        frame->Begin();
    }
}

bool DeviceVulkan::IsSwapchainTouched()
{
    return wsi.consumed;
}

void DeviceVulkan::SubmitImpl(CommandListPtr& cmd, FencePtr* fence, U32 semaphoreCount, SemaphorePtr* semaphore)
{
    cmd->EndCommandBuffer();

    QueueIndices queueIndex = ConvertQueueTypeToIndices(cmd->GetQueueType());
    auto& submissions = CurrentFrameResource().submissions[queueIndex];
    submissions.push_back(std::move(cmd));

    if (fence != nullptr || (semaphoreCount > 0 && semaphore != nullptr))
    {
        InternalFence signalledFence;
        SubmitQueue(queueIndex, fence != nullptr ? &signalledFence : nullptr, semaphoreCount, semaphore);
    }
}

void DeviceVulkan::SubmitQueue(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores)
{
    // Flush/Submit transfer queue
    if (queueIndex != QueueIndices::QUEUE_INDEX_TRANSFER)
        FlushFrame(QueueIndices::QUEUE_INDEX_TRANSFER);

    auto& submissions = CurrentFrameResource().submissions[queueIndex];
    if (submissions.empty())
    {
        if (fence != nullptr || (semaphoreCount > 0 && semaphores != nullptr))
            SubmitEmpty(queueIndex, fence, semaphoreCount, semaphores);
        return;
    }

    BatchComposer batchComposer;

    // Colloect wait semaphores
    WaitSemaphores waitSemaphores = {};
    QueueData& queueData = queueDatas[queueIndex];
    for (int i = 0; i < queueData.waitSemaphores.size(); i++)
    {
        SemaphorePtr& semaphore = queueData.waitSemaphores[i];
        VkSemaphore vkSemaphore = semaphore->Consume();
        if (semaphore->GetTimeLine() <= 0)
        {
            if (semaphore->CanRecycle())
                RecycleSemaphore(vkSemaphore);
            else
                ReleaseSemaphore(vkSemaphore);

            waitSemaphores.binaryWaits.push_back(vkSemaphore);
            waitSemaphores.binaryWaitStages.push_back(queueData.waitStages[i]);
        }
    }
    queueData.waitSemaphores.clear();
    queueData.waitStages.clear();
    batchComposer.AddWaitSemaphores(waitSemaphores);

    // Compose submit batches
    VkQueue queue = queueInfo.queues[queueIndex];
    for (int i = 0; i < submissions.size(); i++)
    {
        auto& cmd = submissions[i];
        VkPipelineStageFlags stages = cmd->GetSwapchainStages();  

        // For first command buffer which uses WSI, need to emit WSI acquire wait 
        if (stages != 0 && !wsi.consumed)
        {
            // Acquire semaphore
            if (wsi.acquire && wsi.acquire->GetSemaphore() != VK_NULL_HANDLE)
            {
                ASSERT(wsi.acquire->IsSignalled());
                batchComposer.AddWaitSemaphore(wsi.acquire, stages);
                if (wsi.acquire->GetTimeLine() <= 0)
                {
                    if (wsi.acquire->CanRecycle())
                        RecycleSemaphore(wsi.acquire->GetSemaphore());
                    else 
                        ReleaseSemaphore(wsi.acquire->GetSemaphore());
                }

                wsi.acquire->Consume();
                wsi.acquire.reset();
            }

            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());

            // Release semaphore
            VkSemaphore release = semaphoreManager.Requset();
            wsi.release = SemaphorePtr(semaphorePool.allocate(*this, release, true));
            batchComposer.AddSignalSemaphore(release);

            wsi.presentQueue = queue;
            wsi.consumed = true;
        }
        else
        {
            batchComposer.AddCommandBuffer(cmd->GetCommandBuffer());
        }
    }

    // Add cleared fence
    VkFence clearedFence = fence ? fencePoolManager.Requset() : VK_NULL_HANDLE;
    if (fence)
        fence->fence = clearedFence;

    // Add cleared semaphores, they will signal when submission finished
    for (unsigned i = 0; i < semaphoreCount; i++)
    {
        VkSemaphore semaphore = semaphoreManager.Requset();
        batchComposer.AddSignalSemaphore(semaphore);
        semaphores[i] = SemaphorePtr(semaphorePool.allocate(*this, semaphore, true));
    }

    VkResult ret = SubmitBatches(batchComposer, queue, clearedFence);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Submit queue failed: ");
        return;
    }

    submissions.clear();
}

void DeviceVulkan::SubmitEmpty(QueueIndices queueIndex, InternalFence* fence, U32 semaphoreCount, SemaphorePtr* semaphores)
{
    // Submit empty to wait for fences or sempahores
    // 
    // Colloect wait semaphores
    WaitSemaphores waitSemaphores = {};
    QueueData& queueData = queueDatas[queueIndex];
    for (int i = 0; i < queueData.waitSemaphores.size(); i++)
    {
        SemaphorePtr& semaphore = queueData.waitSemaphores[i];
        VkSemaphore vkSemaphore = semaphore->Consume();
        if (semaphore->GetTimeLine() <= 0)
        {
            if (semaphore->CanRecycle())
                RecycleSemaphore(vkSemaphore);
            else
                ReleaseSemaphore(vkSemaphore);

            waitSemaphores.binaryWaits.push_back(vkSemaphore);
            waitSemaphores.binaryWaitStages.push_back(queueData.waitStages[i]);
        }
    }
    queueData.waitSemaphores.clear();
    queueData.waitStages.clear();

    BatchComposer batchComposer;
    batchComposer.AddWaitSemaphores(waitSemaphores);

    // Add cleared fence
    VkFence clearedFence = fence ? fencePoolManager.Requset() : VK_NULL_HANDLE;
    if (fence)
        fence->fence = clearedFence;

    // Add cleared semaphores, they will signal when submission finished
    for (unsigned i = 0; i < semaphoreCount; i++)
    {
        VkSemaphore semaphore = semaphoreManager.Requset();
        batchComposer.AddSignalSemaphore(semaphore);
        semaphores[i] = SemaphorePtr(semaphorePool.allocate(*this, semaphore, true));
    }

    // Submit batches
    VkQueue queue = queueInfo.queues[queueIndex];
    VkResult ret = SubmitBatches(batchComposer, queue, clearedFence);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Submit queue failed: ");
        return;
    }
    queueData.needFence = true;
}

VkResult DeviceVulkan::SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence)
{
    auto& submits = composer.Bake();
    return vkQueueSubmit(queue, (uint32_t)submits.size(), submits.data(), fence);
}

void DeviceVulkan::SubmitStaging(CommandListPtr& cmd, VkBufferUsageFlags usage, bool flush)
{
    // Check source buffer's usage to decide which queues (Graphics/Compute) need to wait
    VkPipelineStageFlags stages = Buffer::BufferUsageToPossibleStages(usage);
    QueueIndices srcQueueIndex = ConvertQueueTypeToIndices(cmd->GetQueueType());
    if (srcQueueIndex == QUEUE_INDEX_GRAPHICS)
    {
    }
    else if (srcQueueIndex == QUEUE_INDEX_COMPUTE)
    {
    }
    else
    {
        auto computeStages = stages &
                (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                 VK_PIPELINE_STAGE_TRANSFER_BIT |
                 VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

        if (stages != 0 && computeStages != 0)
        {
            SemaphorePtr semaphores[2];
            Submit(cmd, nullptr, 2, semaphores);
            AddWaitSemaphore(QueueType::QUEUE_TYPE_GRAPHICS, semaphores[0], stages, flush);
            AddWaitSemaphore(QueueType::QUEUE_TYPE_ASYNC_COMPUTE, semaphores[1], computeStages, flush);
        }
        else if (stages != 0)
        {
            SemaphorePtr semaphores;
            Submit(cmd, nullptr, 1, &semaphores);
            AddWaitSemaphore(QueueType::QUEUE_TYPE_GRAPHICS, semaphores, stages, flush);
        }
        else if (computeStages != 0)
        {
            SemaphorePtr semaphores;
            Submit(cmd, nullptr, 1, &semaphores);
            AddWaitSemaphore(QueueType::QUEUE_TYPE_ASYNC_COMPUTE, semaphores, stages, flush);
        }
        else
        {
            Submit(cmd, nullptr, 0, nullptr);
        }
    }
}

DeviceVulkan::FrameResource::FrameResource(DeviceVulkan& device_) : device(device_)
{
    const int THREAD_COUNT = 1;
    for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
    {
        cmdPools[queueIndex].reserve(THREAD_COUNT);
        for (int threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
            cmdPools[queueIndex].emplace_back(&device_, device_.queueInfo.familyIndices[queueIndex]);
    }
}

DeviceVulkan::FrameResource::~FrameResource()
{
    Begin();
}

void DeviceVulkan::FrameResource::Begin()
{
    VkDevice vkDevice = device.device;

    // wait for submiting
    if (!waitFences.empty())
    {
        vkWaitForFences(device.device, (uint32_t)waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
        waitFences.clear();
    }

    // reset recyle fences
    if (!recyleFences.empty())
    {
        vkResetFences(device.device, (uint32_t)recyleFences.size(), recyleFences.data());
        for (auto& fence : recyleFences)
            device.fencePoolManager.Recyle(fence);
        recyleFences.clear();
    }

    // reset command pools
    for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
    {
        for (auto& pool : cmdPools[queueIndex])
            pool.BeginFrame();
    }

    // clear destroyed resources
    for (auto& buffer : destroyedFrameBuffers)
        vkDestroyFramebuffer(vkDevice, buffer, nullptr);
    for (auto& image : destroyedImages)
        vkDestroyImage(vkDevice, image, nullptr);
    for (auto& imageView : destroyedImageViews)
        vkDestroyImageView(vkDevice, imageView, nullptr);
    for (auto& buffer : destroyedBuffers)
        vkDestroyBuffer(vkDevice, buffer, nullptr);
    for (auto& bufferView : destroyedBufferViews)
        vkDestroyBufferView(vkDevice, bufferView, nullptr);
    for (auto& semaphore : destroyeSemaphores)
        vkDestroySemaphore(vkDevice, semaphore, nullptr);
    for (auto& pipeline : destroyedPipelines)
        vkDestroyPipeline(vkDevice, pipeline, nullptr);
    for (auto& allocation : destroyedAllocations)
        allocation.Free(device.memory);

    destroyedFrameBuffers.clear();
    destroyedImages.clear();
    destroyedImageViews.clear();
    destroyedBuffers.clear();
    destroyedBufferViews.clear();
    destroyeSemaphores.clear();
    destroyedPipelines.clear();
    destroyedAllocations.clear();

    // reset recyle semaphores
    auto& recyleSemaphores = recycledSemaphroes;
    for (auto& semaphore : recyleSemaphores)
        device.semaphoreManager.Recyle(semaphore);
    recyleSemaphores.clear();
}

BatchComposer::BatchComposer()
{
    // 默认存在一个Submit
    submits.emplace_back();
}

void BatchComposer::BeginBatch()
{
    // create a new VkSubmitInfo
    auto& submitInfo = submitInfos[submitIndex];
    if (!submitInfo.commandLists.empty() || !submitInfo.waitSemaphores.empty())
    {
        submitIndex = (uint32_t)submits.size();
        submits.emplace_back();
    }
}

void BatchComposer::AddWaitSemaphores(WaitSemaphores& semaphores)
{
    if (!semaphores.binaryWaits.empty())
    {
        for (int i = 0; i < semaphores.binaryWaits.size(); i++)
        {
            submitInfos[submitIndex].waitSemaphores.push_back(semaphores.binaryWaits[i]);
            submitInfos[submitIndex].waitStages.push_back(semaphores.binaryWaitStages[i]);    
        }
    }
}

void BatchComposer::AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages)
{
    // 添加wait semaphores时先将之前的cmds batch
    if (!submitInfos[submitIndex].commandLists.empty())
        BeginBatch();

    submitInfos[submitIndex].waitSemaphores.push_back(sem->GetSemaphore());
    submitInfos[submitIndex].waitStages.push_back(stages);
}

void BatchComposer::AddSignalSemaphore(VkSemaphore sem)
{
    submitInfos[submitIndex].signalSemaphores.push_back(sem);
}

void BatchComposer::AddCommandBuffer(VkCommandBuffer buffer)
{
    // 如果存在Signal semaphores，先将之前的cmds batch
    if (!submitInfos[submitIndex].signalSemaphores.empty())
        BeginBatch();

    submitInfos[submitIndex].commandLists.push_back(buffer);
}

std::vector<VkSubmitInfo>& BatchComposer::Bake()
{
    // setup VKSubmitInfos
    for (size_t index = 0; index < submits.size(); index++)
    {
        auto& vkSubmitInfo = submits[index];
        auto& submitInfo = submitInfos[index];
        vkSubmitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        vkSubmitInfo.commandBufferCount = (uint32_t)submitInfo.commandLists.size();
        vkSubmitInfo.pCommandBuffers = submitInfo.commandLists.data();

        vkSubmitInfo.waitSemaphoreCount = (uint32_t)submitInfo.waitSemaphores.size();
        vkSubmitInfo.pWaitSemaphores = submitInfo.waitSemaphores.data();
        vkSubmitInfo.pWaitDstStageMask = submitInfo.waitStages.data();

        vkSubmitInfo.signalSemaphoreCount = (uint32_t)submitInfo.signalSemaphores.size();
        vkSubmitInfo.pSignalSemaphores = submitInfo.signalSemaphores.data();
    }

    return submits;
}

namespace {
    static void UpdateShaderArrayInfo(ShaderResourceLayout& layout, U32 set, U32 binding)
    {
        
    }
}

bool DeviceVulkan::ReflectShader(ShaderResourceLayout& layout, const U32 *spirvData, size_t spirvSize)
{
	SpvReflectShaderModule module;
	if (spvReflectCreateShaderModule(spirvSize, spirvData, &module) != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to create reflect shader module.");
		return false;
	}

	// get bindings info
	U32 bindingCount = 0;
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr))
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}
	std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
	if (spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data()))
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}

	// get push constants info
	U32 pushCount = 0;
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, nullptr))
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}
	std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
	if (spvReflectEnumeratePushConstantBlocks(&module, &pushCount, pushConstants.data()))
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}

    // parse push constant buffers
    if (!pushConstants.empty())
    {
        // 这里仅仅获取第一个constant的大小
        // At least on older validation layers, it did not do a static analysis to determine similar information
        layout.pushConstantSize = pushConstants.front()->offset + pushConstants.front()->size;
    }

    // parse bindings
    for (auto& x : bindings)
    {
        auto descriptorType = x->descriptor_type;
        U32 set = x->set;
        U32 binding = x->binding;
        if (descriptorType == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        {
            layout.sets[set].masks[static_cast<U32>(DescriptorSetLayout::STORAGE_IMAGE)] |= 1u << binding;
            UpdateShaderArrayInfo(layout, set, binding);
        }
    }

	return true;
}

void DeviceVulkan::InitBindless()
{
    DescriptorSetLayout layout;
    layout.arraySize[0] = DescriptorSetLayout::UNSIZED_ARRAY;
    for (unsigned i = 1; i < VULKAN_NUM_BINDINGS; i++)
        layout.arraySize[i] = 1;

    uint32_t stagesForSets[VULKAN_NUM_BINDINGS] = { VK_SHADER_STAGE_ALL };

    if (features.features_1_2.descriptorBindingUniformTexelBufferUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[(U32)DescriptorSetLayout::SetMask::SAMPLED_IMAGE] = 1;
        bindlessSampledImages = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
    if (features.features_1_2.descriptorBindingStorageBufferUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[(U32)DescriptorSetLayout::SetMask::STORAGE_BUFFER] = 1;
        bindlessStorageBuffers = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
    if (features.features_1_2.descriptorBindingStorageImageUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[(U32)DescriptorSetLayout::SetMask::STORAGE_IMAGE] = 1;
        bindlessStorageImages = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
    if (features.features_1_2.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE)
    {
        DescriptorSetLayout tmpLayout = layout;
        tmpLayout.masks[(U32)DescriptorSetLayout::SetMask::SAMPLER] = 1;
        bindlessSamplers = &RequestDescriptorSetAllocator(tmpLayout, stagesForSets);
    }
}

void TransientAttachmentAllcoator::BeginFrame()
{
    attachments.BeginFrame();
}

void TransientAttachmentAllcoator::Clear()
{
    attachments.Clear();
}

ImagePtr TransientAttachmentAllcoator::RequsetAttachment(U32 w, U32 h, VkFormat format, U32 index, U32 samples, U32 layers)
{
    HashCombiner hash;
    hash.HashCombine(w);
    hash.HashCombine(h);
    hash.HashCombine(format);
    hash.HashCombine(index);
    hash.HashCombine(samples);
    hash.HashCombine(layers);

    auto* node = attachments.Requset(hash.Get());
    if (node != nullptr)
        return node->image;

    auto imageInfo = ImageCreateInfo::TransientRenderTarget(w, h, format);
    imageInfo.samples = static_cast<VkSampleCountFlagBits>(samples);
    imageInfo.layers = layers;

    node = attachments.Emplace(hash.Get(), device.CreateImage(imageInfo, nullptr));
    return node->image;
}

void FrameBufferAllocator::BeginFrame()
{
    framebuffers.BeginFrame();
}

void FrameBufferAllocator::Clear()
{
    framebuffers.Clear();
}

FrameBuffer& FrameBufferAllocator::RequestFrameBuffer(const RenderPassInfo& info)
{
    HashCombiner hash;
    RenderPass& renderPass = device.RequestRenderPass(info, true);
    hash.HashCombine(renderPass.GetHash());

    // Get color attachments hash
    for (uint32_t i = 0; i < info.numColorAttachments; i++)
        hash.HashCombine(info.colorAttachments[i]->GetCookie());

    // Get depth stencil hash
    if (info.depthStencil)
        hash.HashCombine(info.depthStencil->GetCookie());

    FrameBufferNode* node = framebuffers.Requset(hash.Get());
    if (node != nullptr)
        return *node;

    return *framebuffers.Emplace(hash.Get(), device, renderPass, info);
}

}