#include "device.h"
#include "utils\hash.h"

#include "spirv_reflect\spirv_reflect.h"

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
}

DeviceVulkan::DeviceVulkan() :
    shaderManager(*this)
{
 

    ///////////////////////////////////////////////////////////////////////////////////////////
    // create frame resources
    const int FRAME_COUNT = 2;
    const int THREAD_COUNT = 1;
    for (int frameIndex = 0; frameIndex < FRAME_COUNT; frameIndex++)
    {

        auto& frameResource = frameResources.emplace_back();
        for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
        {
            frameResource.cmdPools->reserve(THREAD_COUNT);
            for (int threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
            {
                frameResource.cmdPools->emplace_back(this, queueIndex);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    //init managers
    fencePoolManager.Initialize(*this);
    semaphoreManager.Initialize(*this);
}

DeviceVulkan::~DeviceVulkan()
{
    fencePoolManager.ClearAll();
    semaphoreManager.ClearAll();
}

void DeviceVulkan::SetContext(VulkanContext& context)
{
    device = context.device;
    physicalDevice = context.physicalDevice;
    instance = context.instance;
    queueInfo = context.queueInfo;

    ///////////////////////////////////////////////////////////////////////////////////////////
    // create frame resources
    const int FRAME_COUNT = 2;
    const int THREAD_COUNT = 1;
    for (int frameIndex = 0; frameIndex < FRAME_COUNT; frameIndex++)
    {

        auto& frameResource = frameResources.emplace_back();
        for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
        {
            frameResource.cmdPools->reserve(THREAD_COUNT);
            for (int threadIndex = 0; threadIndex < THREAD_COUNT; threadIndex++)
            {
                frameResource.cmdPools->emplace_back(this, queueIndex);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////
    //init managers
    fencePoolManager.Initialize(*this);
    semaphoreManager.Initialize(*this);
}

bool DeviceVulkan::CreateSwapchain(Swapchain*& swapchain, VkSurfaceKHR surface, uint32_t width, uint32_t height)
{
    if (surface == VK_NULL_HANDLE)
        return false;

    if (swapchain != nullptr)
        delete swapchain;

    swapchain = new Swapchain(device);

    // ??????????
    bool useSurfaceInfo = false; // mExtensionFeatures.SupportsSurfaceCapabilities2;
    if (useSurfaceInfo)
    {
    }
    else
    {
        VkSurfaceCapabilitiesKHR surfaceProperties = {};
        auto res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceProperties);
        assert(res == VK_SUCCESS);

        // get surface formats
        uint32_t formatCount;
        res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        assert(res == VK_SUCCESS);

        if (formatCount != 0)
        {
            swapchain->formats.resize(formatCount);
            res = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapchain->formats.data());
            assert(res == VK_SUCCESS);
        }

        // get present mode
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            swapchain->presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, swapchain->presentModes.data());
        }

        // find suitable surface format
        VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_UNDEFINED };
        VkFormat targetFormat = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;
        for (auto& format : swapchain->formats)
        {
            if (format.format = targetFormat)
            {
                surfaceFormat = format;
                break;
            }
        }

        if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            Logger::Error("Failed to find suitable surface format.");
            return false;
        }
        swapchain->format = surfaceFormat;

        // find suitable present mode
        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // �������Ǹ����У���ʾ��ʱ��Ӷ���ͷ��һ��ͼ�񣬳��������Ⱦ��ͼ�񵽶���β��
        bool isVsync = true;                                              // ����������˳����Ҫ�ȴ����������Ǵ�ֱͬ������ʾˢ�µ�ʱ�̾��Ǵ�ֱ�հ�
        if (!isVsync)
        {
            for (auto& presentMode : swapchain->presentModes)
            {
                if (presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    swapchainPresentMode = presentMode;
                    break;
                }
            }
        }

        // set swapchin extent
        swapchain->swapchainExtent = { width, height };
        swapchain->swapchainExtent.width =
            max(surfaceProperties.minImageExtent.width, min(surfaceProperties.maxImageExtent.width, width));
        swapchain->swapchainExtent.height =
            max(surfaceProperties.minImageExtent.height, min(surfaceProperties.maxImageExtent.height, height));

        // create swapchain
        uint32_t desiredSwapchainImages = 2;
        if (desiredSwapchainImages < surfaceProperties.minImageCount)
            desiredSwapchainImages = surfaceProperties.minImageCount;
        if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
            desiredSwapchainImages = surfaceProperties.maxImageCount;


        VkSwapchainKHR oldSwapchain = swapchain->swapChain;;
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = desiredSwapchainImages;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = swapchain->swapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;            // һ��ͼ����ĳ��ʱ����ֻ�ܱ�һ��������ռ�ã��ڱ���һ��������ʹ��ǰ������ռ�����һ��Ҫ��ʽ�ؽ���ת�ơ���ѡ���ṩ����õ����ܡ�
        createInfo.preTransform = surfaceProperties.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;      // ��alphaͨ���Ƿ�Ӧ�úʹ���ϵͳ�е��������ڽ��л��, Ĭ�ϲ����
        createInfo.presentMode = swapchainPresentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapchain;

        res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain->swapChain);
        assert(res == VK_SUCCESS);

        uint32_t imageCount = 0;
        res = vkGetSwapchainImagesKHR(device, swapchain->swapChain, &imageCount, nullptr);
        assert(res == VK_SUCCESS);

        std::vector<VkImage> images(imageCount);
        res = vkGetSwapchainImagesKHR(device, swapchain->swapChain, &imageCount, images.data());
        assert(res == VK_SUCCESS);

        /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // create image views
        ImageCreateInfo imageCreateInfo = ImageCreateInfo::renderTarget(width, height, swapchain->format.format);
        for (int i = 0; i < images.size(); i++)
        {
            VkImageView imageView;
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = images[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchain->format.format;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            res = vkCreateImageView(device, &createInfo, nullptr, &imageView);
            assert(res == VK_SUCCESS);

            ImagePtr imagePtr = ImagePtr(imagePool.allocate(*this, images[i], imageView, imageCreateInfo));
            if (imagePtr)
            {
                imagePtr->DisownImge(); // image��vkGetSwapchainImagesKHR��ȡ
                imagePtr->SetSwapchainLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
                swapchain->images.push_back(imagePtr);
            }

            // ��GetSwapchainRenderPassInfo�и���Hash��ȡ
            //VkImageView attachments[] = {
            //    swapchain->mImageViews[i]
            //};
            //// ����frameBuffer��ImageViewһһ��Ӧ
            //VkFramebufferCreateInfo framebufferInfo = {};
            //res = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain->frameBuffers[i]);
            //assert(res == VK_SUCCESS);
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

RenderPassInfo DeviceVulkan::GetSwapchianRenderPassInfo(const Swapchain& swapChain, SwapchainRenderPassType swapchainRenderPassType)
{
    RenderPassInfo info = {};
    info.numColorAttachments = 1;
    info.colorAttachments[0] = &swapChain.images[swapChain.mImageIndex]->GetImageView();
    info.clearAttachments = ~0u;
    info.storeAttachments = 1u << 0;

    return info;
}

RenderPass& DeviceVulkan::RequestRenderPass(const RenderPassInfo& renderPassInfo, bool isComatible)
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

    auto findIt = renderPasses.find(hash.Get());
    if (findIt != renderPasses.end())
        return *findIt->second;

    RenderPass& renderPass = *renderPasses.emplace(hash.Get(), *this, renderPassInfo);
    renderPass.SetHash(hash.Get());
    return renderPass;
}

FrameBuffer& DeviceVulkan::RequestFrameBuffer(const RenderPassInfo& renderPassInfo)
{
    HashCombiner hash;

    // ��Ҫ����RenderPass�Լ�RT��Hashֵ��ȡ���ߴ���FrameBuffer
    RenderPass& renderPass = RequestRenderPass(renderPassInfo, true);
    hash.HashCombine(renderPass.GetHash());

    // Get color attachments hash
    for (uint32_t i = 0; i < renderPassInfo.numColorAttachments; i++)
        hash.HashCombine(renderPassInfo.colorAttachments[i]->GetCookie());

    // Get depth stencil hash
    if (renderPassInfo.depthStencil)
        hash.HashCombine(renderPassInfo.depthStencil->GetCookie());

    auto findIt = frameBuffers.find(hash.Get());
    if (findIt != frameBuffers.end())
        return *findIt->second;

    auto& frameBuffer = *frameBuffers.emplace(hash.Get(), *this, renderPass, renderPassInfo);
    frameBuffer.SetHash(hash.Get());
    return frameBuffer;
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
	
	auto findIt = descriptorSetAllocators.find(hasher.Get());
	if (findIt != descriptorSetAllocators.end())
		return *findIt->second;

    DescriptorSetAllocator* allocator = descriptorSetAllocators.emplace(hasher.Get(), *this, layout, stageForBinds);
    allocator->SetHash(hasher.Get());
	return *allocator;
}

void DeviceVulkan::BeginFrameContext()
{
    // submit remain queue
    EndFrameContext();

    // descriptor set begin
    for (auto& kvp : descriptorSetAllocators)
        kvp.second->BeginFrame();

    // update frameIndex
    if (++frameIndex >= frameResources.size())
        frameIndex = 0;

    // reset recyle fences
    auto& recyleFences = CurrentFrameResource().recyleFences;
    if (!recyleFences.empty())
    {
        vkResetFences(device, (uint32_t)recyleFences.size(), recyleFences.data());
        for (auto& fence : recyleFences)
            fencePoolManager.Recyle(fence);
        recyleFences.clear();
    }

    // begin frame resources
    CurrentFrameResource().Begin(device);

    // clear destroyed resources
    CurrentFrameResource().ProcessDestroyed(device);

    // reset recyle semaphores
    auto& recyleSemaphores = CurrentFrameResource().recycledSemaphroes;
    for (auto& semaphore : recyleSemaphores)
        semaphoreManager.Recyle(semaphore);
    recyleSemaphores.clear();
}

void DeviceVulkan::EndFrameContext()
{
    // flush queue
    InternalFence fence;
    auto& frame = CurrentFrameResource();
    auto submissionList = frame.submissions;
    for (auto& queueIndex : QUEUE_FLUSH_ORDER)
    {
        if (!submissionList[queueIndex].empty())
        {
            SubmitQueue(queueIndex, &fence);
           
            // ���������Fence�������ӵ�waitFences�������BeginFrameǰ�ȴ�
            if (fence.fence != VK_NULL_HANDLE)
            {
                frame.waitFences.push_back(fence.fence);
                frame.recyleFences.push_back(fence.fence);
            }
        }
    }
}

void DeviceVulkan::Submit(CommandListPtr& cmd)
{
    cmd->EndCommandBuffer();

    QueueIndices queueIndex = ConvertQueueTypeToIndices(cmd->GetQueueType());
    auto& submissions = CurrentFrameResource().submissions[queueIndex];
    submissions.push_back(std::move(cmd));
}

void DeviceVulkan::SetAcquireSemaphore(uint32_t index, SemaphorePtr acquire)
{
    wsi.acquire = std::move(acquire);
    wsi.index = index;
    wsi.consumed = false;
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

void DeviceVulkan::ReleaseSemaphore(VkSemaphore semaphore, bool isSignalled)
{
    // �Ѿ�signalled��semaphore��ֱ�����٣�����ѭ��ʹ��
    if (isSignalled)
    {
        CurrentFrameResource().destroyeSemaphores.push_back(semaphore);
    }
    else
    {
        CurrentFrameResource().recycledSemaphroes.push_back(semaphore);
    }
}

void DeviceVulkan::ReleasePipeline(VkPipeline pipeline)
{
    CurrentFrameResource().destroyedPipelines.push_back(pipeline);
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

void DeviceVulkan::UpdateGraphicsPipelineHash(CompilePipelineState pipeline)
{
    HashCombiner hash;


    pipeline.hash = hash.Get();
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
            for (U32 maskbit = 0; i < static_cast<U32>(DescriptorSetLayout::SetMask::COUNT); maskbit++)
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
            resLayout.pushConstantRange.size = max(resLayout.pushConstantRange.size, shaderResLayout.pushConstantSize);
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
    
    auto piplineLayout = RequestPipelineLayout(resLayout);
    program.SetPipelineLayout(&piplineLayout);
}

void DeviceVulkan::SubmitQueue(QueueIndices queueIndex, InternalFence* fence)
{
    auto& submissions = CurrentFrameResource().submissions[queueIndex];

    // ����ύ����Ϊ�գ���ֱ�ӿ��ύ
    if (submissions.empty())
    {
        if (fence != nullptr)
            SubmitEmpty(queueIndex, fence);
        return;
    }

    VkQueue queue = queueInfo.mQueues[queueIndex];
    BatchComposer batchComposer;
    for (int i = 0; i < submissions.size(); i++)
    {
        auto& cmd = submissions[i];
        VkPipelineStageFlags stages = cmd->GetSwapchainStages();  

        // use swapchian in stages
        if (stages != 0 && !wsi.consumed)
        {
            // Acquire semaphore
            if (wsi.acquire && wsi.acquire->GetSemaphore() != VK_NULL_HANDLE)
            {
                batchComposer.AddWaitSemaphore(wsi.acquire, stages);
                if (wsi.acquire->GetTimeLine() <= 0)
                {
                    ReleaseSemaphore(wsi.acquire->GetSemaphore(), wsi.acquire->IsSignalled());
                }

                wsi.acquire->Release();
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

    VkFence clearedFence = fence ? fencePoolManager.Requset() : VK_NULL_HANDLE;
    if (fence)
        fence->fence = clearedFence;

    VkResult ret = SubmitBatches(batchComposer, queue, clearedFence);
    if (ret != VK_SUCCESS)
    {
        Logger::Error("Submit queue failed: ");
        return;
    }

    submissions.clear();
}

void DeviceVulkan::SubmitEmpty(QueueIndices queueIndex, InternalFence* fence)
{
}

VkResult DeviceVulkan::SubmitBatches(BatchComposer& composer, VkQueue queue, VkFence fence)
{
    auto& submits = composer.Bake();
    return vkQueueSubmit(queue, (uint32_t)submits.size(), submits.data(), fence);
}

void DeviceVulkan::FrameResource::Begin(VkDevice device)
{
    // wait for submiting
    if (!waitFences.empty())
    {
        vkWaitForFences(device, (uint32_t)waitFences.size(), waitFences.data(), VK_TRUE, UINT64_MAX);
        waitFences.clear();
    }

    // reset command pools
    for (int queueIndex = 0; queueIndex < QUEUE_INDEX_COUNT; queueIndex++)
    {
        for (auto& pool : cmdPools[queueIndex])
            pool.BeginFrame();
    }
}

void DeviceVulkan::FrameResource::ProcessDestroyed(VkDevice device)
{
	for (auto& buffer : destroyedFrameBuffers)
		vkDestroyFramebuffer(device, buffer, nullptr);
    for (auto& image : destroyedImages)
        vkDestroyImage(device, image, nullptr);
    for (auto& imageView : destroyedImageViews)
		vkDestroyImageView(device, imageView, nullptr);
    for (auto& semaphore : destroyeSemaphores)
        vkDestroySemaphore(device, semaphore, nullptr);
    for (auto& pipeline : destroyedPipelines)
        vkDestroyPipeline(device, pipeline, nullptr);

	destroyedFrameBuffers.clear();
	destroyedImages.clear();
	destroyedImageViews.clear();
    destroyeSemaphores.clear();
    destroyedPipelines.clear();
}

BatchComposer::BatchComposer()
{
    // Ĭ�ϴ���һ��Submit
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

void BatchComposer::AddWaitSemaphore(SemaphorePtr& sem, VkPipelineStageFlags stages)
{
    // ����wait semaphoresʱ�Ƚ�֮ǰ��cmds batch
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
    // �������Signal semaphores���Ƚ�֮ǰ��cmds batch
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

bool DeviceVulkan::ReflectShader(ShaderResourceLayout& layout, const U32 *spirvData, size_t spirvSize);
{
	SpvReflectShaderModule module;
	if (spvReflectCreateShaderModule(spirvSize, spirvData, &module != SPV_REFLECT_RESULT_SUCCESS)
	{
		Logger::Error("Failed to create reflect shader module.");
		return false;
	}

	// get bindings info
	U32 bindingCount = 0;
	if (spvReflectEnumerateDescriptorBindings(module, &bindingCount, nullptr)
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}
	std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
	if (spvReflectEnumerateDescriptorBindings(module, &bindingCount, bindings.data())
	{
		Logger::Error("Failed to reflect bindings.");
		return false;
	}

	// get push constants info
	U32 pushCount = 0;
	if (spvReflectEnumeratePushConstantBlocks(module, &pushCount, nullptr)
	{
		Logger::Error("Failed to reflect push constant blocks.");
		return false;
	}
	std::vector<SpvReflectBlockVariable*> pushConstants(pushCount);
	if (spvReflectEnumeratePushConstantBlocks(module, &pushCount, pushConstants.data())
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
            layout.sets[set][static_cast<U32>(DescriptorSetLayout::STORAGE_IMAGE)] |= 1u << binding;
            UpdateShaderArrayInfo(layout, set, binding);
        }
    }

	return true;
}

}