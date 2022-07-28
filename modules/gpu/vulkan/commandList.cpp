#include "commandList.h"
#include "vulkan/device.h"
#include "core\platform\platform.h"

namespace VulkanTest
{
namespace GPU
{

CommandPool::CommandPool(DeviceVulkan* device_, U32 queueFamilyIndex) :
    device(device_)
{
    VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = queueFamilyIndex;

    if (queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        VkResult res = vkCreateCommandPool(device->device, &info, nullptr, &pool);
        ASSERT(res == VK_SUCCESS);
    }
}

CommandPool::~CommandPool()
{
    if (!buffers.empty())
        vkFreeCommandBuffers(device->device, pool, (U32)buffers.size(), buffers.data());

    if (pool != VK_NULL_HANDLE)
        vkDestroyCommandPool(device->device, pool, nullptr);
}

CommandPool::CommandPool(CommandPool&& other) noexcept
{
    *this = std::move(other);
}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept
{
    if (this != &other)
    {
        device = other.device;
        if (!buffers.empty())
            vkFreeCommandBuffers(device->device, pool, (U32)buffers.size(), buffers.data());
        if (pool != VK_NULL_HANDLE)
            vkDestroyCommandPool(device->device, pool, nullptr);

        pool = VK_NULL_HANDLE;
        buffers.clear();
        usedIndex = other.usedIndex;
        other.usedIndex = 0;

        std::swap(pool, other.pool);
        std::swap(buffers, other.buffers);
    }

    return *this;
}

VkCommandBuffer CommandPool::RequestCommandBuffer()
{
    ASSERT(pool != VK_NULL_HANDLE);
    if (usedIndex < buffers.size())
        return buffers[usedIndex++];

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(device->device, &info, &cmd);
    ASSERT(res == VK_SUCCESS);

    buffers.push_back(cmd);
    usedIndex++;

    return cmd;
}

void CommandPool::BeginFrame()
{
    if (pool == VK_NULL_HANDLE)
        return;

    if (usedIndex > 0)
        vkResetCommandPool(device->device, pool, 0);
    usedIndex = 0;
}

void CommandListDeleter::operator()(CommandList* cmd)
{
    if (cmd != nullptr)
        cmd->device.commandListPool.free(cmd);
}

CommandList::CommandList(DeviceVulkan& device_, VkCommandBuffer buffer_, QueueType type_, VkPipelineCache cache_) :
    device(device_),
    cmd(buffer_),
    type(type_)
{
    pipelineState.cache = cache_;
    BeginCompute();
    memset(&bindings, 0, sizeof(bindings));
}

CommandList::~CommandList()
{
    ASSERT(vboBlock.mapped == nullptr);
    ASSERT(iboBlock.mapped == nullptr);
    ASSERT(uboBlock.mapped == nullptr);
    ASSERT(stagingBlock.mapped == nullptr);
    ASSERT(storageBlock.mapped == nullptr);
}

void CommandList::BeginRenderPass(const RenderPassInfo& renderPassInfo, VkSubpassContents contents)
{
    ASSERT(frameBuffer == nullptr);
    ASSERT(compatibleRenderPass == nullptr);

    // Get frame buffer and render pass
    frameBuffer = &device.RequestFrameBuffer(renderPassInfo);
    compatibleRenderPass = &frameBuffer->GetRenderPass();
    renderPass = &device.RequestRenderPass(renderPassInfo);

    pipelineState.subpassIndex = 0;

    // init framebuffer attachments
    memset(frameBufferAttachments, 0, sizeof(frameBufferAttachments));
    U32 att = 0;
    for (att = 0; att < renderPassInfo.numColorAttachments; att++)
        frameBufferAttachments[att] = renderPassInfo.colorAttachments[att];
    if (renderPassInfo.depthStencil != nullptr)
        frameBufferAttachments[att] = renderPassInfo.depthStencil;

    // area
    VkRect2D rect = renderPassInfo.renderArea;
    U32 fbWidth = frameBuffer->GetWidth();
    U32 fbHeight = frameBuffer->GetHeight();
    rect.offset.x = std::min(fbWidth, U32(rect.offset.x));
    rect.offset.y = std::min(fbHeight, U32(rect.offset.y));
    rect.extent.width = std::min(fbWidth - rect.offset.x, rect.extent.width);
    rect.extent.height = std::min(fbHeight - rect.offset.y, rect.extent.height);

    viewport = {
        float(rect.offset.x), float(rect.offset.y),
        float(rect.extent.width), float(rect.extent.height),   // Flipping the viewport, because of the Y+ of the NDC is down
        0.0f, 1.0f
    };
    scissor = rect;

    // 遍历所有colorAttachments同时检查clearAttachments mask
    VkClearValue clearColors[VULKAN_NUM_ATTACHMENTS + 1];
    U32 numClearColor = 0;
    for (U32 i = 0; i < renderPassInfo.numColorAttachments; i++)
    {
        if (renderPassInfo.clearAttachments & (1u << i))
        {
            clearColors[i].color = renderPassInfo.clearColor[i];
            numClearColor = i + 1;
        }

        // Check use swapchian in stages
        if (renderPassInfo.colorAttachments[i]->GetImage()->IsSwapchainImage())
            SetSwapchainStages(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);  // 指定混合后管道的阶段，其中从管道输出最终颜色值
                                                                                // 此阶段还包括子颜色加载和存储操作以及具有颜色格式的帧缓冲附件的多重采样解析操作
    }
    // check is depth stencil and clear depth stencil
    if (renderPassInfo.depthStencil != nullptr)
    {
        clearColors[renderPassInfo.numColorAttachments].depthStencil = renderPassInfo.clearDepthStencil;
        numClearColor = renderPassInfo.numColorAttachments + 1;
    }

    // begin render pass
    VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    beginInfo.renderArea = scissor;
    beginInfo.renderPass = renderPass->GetRenderPass();
    beginInfo.framebuffer = frameBuffer->GetFrameBuffer();
    beginInfo.clearValueCount = numClearColor;
    beginInfo.pClearValues = clearColors;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    subpassContents = contents;
    BeginGraphics();
}

void CommandList::EndRenderPass()
{
    vkCmdEndRenderPass(cmd);

    // clear runtime resources
    frameBuffer = nullptr;
    renderPass = nullptr;
    compatibleRenderPass = nullptr;

    // begin graphics contexxt
    BeginCompute();
}

void CommandList::ClearPipelineState()
{
    pipelineState.blendState = {};
    pipelineState.rasterizerState = {};
    pipelineState.depthStencilState = {};
    pipelineState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

void CommandList::SetDefaultOpaqueState()
{
    ClearPipelineState();

    // blend
    BlendState& bd = pipelineState.blendState;
    bd.renderTarget[0].blendEnable = false;
    bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
    bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    bd.renderTarget[0].blendOp = VK_BLEND_OP_MAX;
    bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
    bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ZERO;
    bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
    bd.renderTarget[0].renderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
    bd.alphaToCoverageEnable = false;
    bd.independentBlendEnable = false;

    // depth
    DepthStencilState& dsd = pipelineState.depthStencilState;
    dsd.depthEnable = true;
    dsd.depthWriteMask = DEPTH_WRITE_MASK_ALL;
    dsd.depthFunc = VK_COMPARE_OP_GREATER;
    dsd.stencilEnable = false;

    // rasterizerState
    RasterizerState& rs = pipelineState.rasterizerState;
    rs.fillMode = FILL_SOLID;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontCounterClockwise = true;
    rs.depthBias = 0;
    rs.depthBiasClamp = 0;
    rs.slopeScaledDepthBias = 0;
    rs.depthClipEnable = false;
    rs.multisampleEnable = false;
    rs.antialiasedLineEnable = false;
    rs.conservativeRasterizationEnable = false;

    // topology
    pipelineState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetDefaultTransparentState()
{
    ClearPipelineState();

    // blend
    BlendState& bd = pipelineState.blendState;
    bd.renderTarget[0].blendEnable = true;
    bd.renderTarget[0].srcBlend = VK_BLEND_FACTOR_SRC_ALPHA;
    bd.renderTarget[0].destBlend = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    bd.renderTarget[0].blendOp = VK_BLEND_OP_ADD;
    bd.renderTarget[0].srcBlendAlpha = VK_BLEND_FACTOR_ONE;
    bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    bd.renderTarget[0].blendOpAlpha = VK_BLEND_OP_ADD;
    bd.renderTarget[0].renderTargetWriteMask = COLOR_WRITE_ENABLE_ALL;
    bd.alphaToCoverageEnable = false;
    bd.independentBlendEnable = false;

    // depth
    DepthStencilState& dsd = pipelineState.depthStencilState;
    dsd.depthEnable = true;
    dsd.depthWriteMask = DEPTH_WRITE_MASK_ALL;
    dsd.depthFunc = VK_COMPARE_OP_GREATER;
    dsd.stencilEnable = false;

    // rasterizerState
    RasterizerState& rs = pipelineState.rasterizerState;
    rs.fillMode = FILL_SOLID;
    rs.cullMode = VK_CULL_MODE_BACK_BIT;
    rs.frontCounterClockwise = false;
    rs.depthBias = 0;
    rs.depthBiasClamp = 0;
    rs.slopeScaledDepthBias = 0;
    rs.depthClipEnable = false;
    rs.multisampleEnable = false;
    rs.antialiasedLineEnable = false;
    rs.conservativeRasterizationEnable = false;

    // topology
    pipelineState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetPrimitiveTopology(VkPrimitiveTopology topology)
{
    pipelineState.topology = topology;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetShaderProgram(ShaderProgram* program)
{
    if (pipelineState.shaderProgram == program)
        return;

    pipelineState.shaderProgram = program;
    currentPipeline = VK_NULL_HANDLE;
    SetDirty(COMMAND_LIST_DIRTY_PIPELINE_BIT | COMMAND_LIST_DIRTY_DYNAMIC_BITS);

    if (program == nullptr)
        return;

    if (currentLayout == nullptr)
    {
        dirtySets = ~0u;
        SetDirty(COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT);

        currentLayout = program->GetPipelineLayout();
        currentPipelineLayout = currentLayout->GetLayout();
    }
    else if (program->GetPipelineLayout()->GetHash() != currentLayout->GetHash())
    {
        // check layout is changed
        auto& newResLayout = program->GetPipelineLayout()->GetResLayout();
        auto& oldResLayout = currentLayout->GetResLayout();
        if (newResLayout.pushConstantHash != oldResLayout.pushConstantHash)
        {
            dirtySets = ~0u;
            SetDirty(COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT);
        }
        else
        {
            // find first diferrent descriptor set
            auto newPipelineLayout = program->GetPipelineLayout();
            for (U32 set = 0; set < VULKAN_NUM_DESCRIPTOR_SETS; set++)
            {
                if (newPipelineLayout->GetAllocator(set) != 
                   currentLayout->GetAllocator(set))
                {
                    dirtySets |= ~((1u << set) - 1);
                    break;
                }
            }
        }        

        currentLayout = program->GetPipelineLayout();
        currentPipelineLayout = currentLayout->GetLayout();
    }
}

void CommandList::SetProgram(const std::string& vertex, const std::string& fragment, const ShaderVariantMap& defines)
{
   auto* tempProgram = device.GetShaderManager().RegisterGraphics(vertex, fragment);
   if (tempProgram == nullptr)
   {
       SetShaderProgram(nullptr);
       return;
   }

   auto* variant = tempProgram->RegisterVariant(defines);
   SetShaderProgram(variant->GetProgram());
}

void CommandList::SetProgram(const Shader* vertex, const Shader* fragment)
{
    if (vertex == nullptr || fragment == nullptr)
    {
        SetShaderProgram(nullptr);
        return;
    }

    const Shader* shaders[static_cast<U32>(ShaderStage::Count)];
    memset(shaders, 0, sizeof(shaders));
    shaders[(U32)ShaderStage::VS] = vertex;
    shaders[(U32)ShaderStage::PS] = fragment;
    SetShaderProgram(device.RequestProgram(shaders));
}

void CommandList::SetProgramShaders(const Shader* shaders[static_cast<U32>(ShaderStage::Count)])
{
    SetShaderProgram(device.RequestProgram(shaders));
}

void CommandList::SetProgram(const Shader* compute)
{
    if (compute == nullptr)
    {
        SetShaderProgram(nullptr);
        return;
    }

    const Shader* shaders[static_cast<U32>(ShaderStage::Count)];
    memset(shaders, 0, sizeof(shaders));
    shaders[(U32)ShaderStage::CS] = compute;
    SetShaderProgram(device.RequestProgram(shaders));
}

void CommandList::ResetCommandContext()
{
    dirty = ~0u;  // set all things are dirty
    dirtyVbos = ~0u;
    dirtySets = ~0u;
    memset(bindings.cookies, 0, sizeof(bindings.cookies));
    memset(vbos.buffers, 0, sizeof(vbos.buffers));
    memset(&indexState, 0, sizeof(indexState));
    pipelineState.shaderProgram = nullptr;
    currentPipeline = VK_NULL_HANDLE;
    currentPipelineLayout = VK_NULL_HANDLE;
    currentLayout = nullptr;
}

void CommandList::EndCommandBuffer()
{
    EndCommandBufferForThread();

    if (vboBlock.mapped != nullptr)
        device.RequestVertexBufferBlockNolock(vboBlock, 0);
    if (iboBlock.mapped != nullptr)
        device.RequestIndexBufferBlockNoLock(iboBlock, 0);
    if (uboBlock.mapped != nullptr)
        device.RequestUniformBufferBlockNoLock(uboBlock, 0);
    if (stagingBlock.mapped != nullptr)
        device.RequestStagingBufferBlockNolock(stagingBlock, 0);

    // Strorage block needs to be persisted
    if (storageBlock.mapped != nullptr)
    {
        device.RecordStorageBufferBlock(storageBlock, *this);
        storageBlock = {};
    }
}

void CommandList::SetTextureImpl(U32 set, U32 binding, VkImageView imageView, VkImageLayout layout, U64 cookie, DescriptorSetType setType)
{
    ASSERT(set < VULKAN_NUM_DESCRIPTOR_SETS);
    ASSERT(binding < VULKAN_NUM_BINDINGS);

    if (cookie == bindings.cookies[set][setType][binding] &&
        layout == bindings.bindings[set][setType][binding].image.imageLayout)
        return;

    ResourceBinding& resBinding = bindings.bindings[set][setType][binding];
    resBinding.image.imageView = imageView;
    resBinding.image.imageLayout = layout;
    bindings.cookies[set][setType][binding] = cookie;
    dirtySets |= 1u << set;
}

#if 0
void CommandList::BindPipelineState(const CompiledPipelineState& pipelineState_)
{
    SetShaderProgram(pipelineState_.shaderProgram);
    pipelineState = pipelineState_;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}
#endif

void* CommandList::AllocateVertexBuffer(U32 binding, VkDeviceSize size, VkDeviceSize stride, VkVertexInputRate inputRate)
{
    auto data = vboBlock.Allocate(size);
    if (data.data == nullptr)
    {
        device.RequestVertexBufferBlock(vboBlock, size);
        data = vboBlock.Allocate(size);
    }

    BindVertexBuffer(vboBlock.gpu, binding, data.offset, stride, inputRate);
    return data.data;
}

void* CommandList::AllocateIndexBuffer(VkDeviceSize size, VkIndexType indexType)
{
    auto data = iboBlock.Allocate(size);
    if (data.data == nullptr)
    {
        device.RequestIndexBufferBlock(iboBlock, size);
        data = iboBlock.Allocate(size);
    }

    BindIndexBuffer(iboBlock.gpu, data.offset, indexType);
    return data.data;
}

 void CommandList::SetVertexAttribute(U32 attribute, U32 binding, VkFormat format, VkDeviceSize offset)
 {
    ASSERT(attribute < VULKAN_NUM_VERTEX_ATTRIBS);
    auto& attr = pipelineState.attribs[attribute];
    if (attr.binding != binding || attr.format != format || attr.offset != offset)
        SetDirty(COMMAND_LIST_DIRTY_STATIC_VERTEX_BIT);

    attr.binding = binding;
    attr.format = format;
    attr.offset = offset;
 }

void CommandList::BindVertexBuffer(const BufferPtr& buffer, U32 binding, VkDeviceSize offset, VkDeviceSize stride, VkVertexInputRate inputRate)
{
    ASSERT(binding < VULKAN_NUM_VERTEX_ATTRIBS);

    VkBuffer targetBuffer = buffer->GetBuffer();
    if (vbos.buffers[binding] != targetBuffer || vbos.offsets[binding] != offset)
        dirtyVbos |= 1u << binding;
    if (vbos.strides[binding] != stride)
        SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_STATIC_VERTEX_BIT);

    vbos.buffers[binding] = targetBuffer;
    vbos.offsets[binding] = offset;
    vbos.strides[binding] = stride;
    vbos.inputRate[binding] = inputRate;
}

void CommandList::BindIndexBuffer(const BufferPtr& buffer, VkDeviceSize offset, VkIndexType indexType)
{
    if (indexState.buffer == buffer->GetBuffer() &&
        indexState.offset == offset &&
        indexState.indexType == indexType)
        return;
    
    indexState.buffer = buffer->GetBuffer();
    indexState.offset = offset;
    indexState.indexType = indexType;
    vkCmdBindIndexBuffer(cmd, indexState.buffer, indexState.offset, indexState.indexType);
}

void CommandList::BindConstantBuffer(const BufferPtr& buffer, U32 set, U32 binding, VkDeviceSize offset, VkDeviceSize range)
{
    ASSERT(set < VULKAN_NUM_DESCRIPTOR_SETS);
    ASSERT(binding < VULKAN_NUM_BINDINGS);
    ASSERT(buffer->GetCreateInfo().usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    if (buffer->GetCookie() == bindings.cookies[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding] &&
        range == bindings.bindings[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding].buffer.range)
    {
        // Check offset is different
        auto& b = bindings.bindings[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding];
        if (b.dynamicOffset != offset)
        {
            dirtySetsDynamic |= 1u << set;
            b.dynamicOffset = offset;
        }
    }
    else
    {
        auto& b = bindings.bindings[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding];
        b.dynamicOffset = offset;
        b.buffer = { buffer->GetBuffer(), 0, range };
        bindings.cookies[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding] = buffer->GetCookie();
        dirtySets |= 1u << set;
    }
}

void CommandList::PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range)
{
    ASSERT(offset + range <= VULKAN_PUSH_CONSTANT_SIZE);
    memcpy(bindings.pushConstantData + offset, data, range);
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT);
}

void* CommandList::AllocateConstant(U32 set, U32 binding, VkDeviceSize size)
{
    ASSERT(size < VULKAN_MAX_UBO_SIZE);
    auto data = uboBlock.Allocate(size);
    if (data.data == nullptr)
    {
        device.RequestUniformBufferBlock(uboBlock, size);
        data = uboBlock.Allocate(size);
    }

    BindConstantBuffer(uboBlock.gpu, set, binding, data.offset, data.paddedSize);
    return data.data;
}

BufferBlockAllocation CommandList::AllocateStorageBuffer(VkDeviceSize size)
{
    auto data = storageBlock.Allocate(size);
    if (data.data == nullptr)
    {
        VkDeviceSize newSize = (size + storageBlock.capacity) * 2.0f;
        device.RequestStorageBufferBlock(storageBlock, newSize);
        data = storageBlock.Allocate(size);
    }
    return data;
}

void* CommandList::UpdateBuffer(const Buffer& buffer, VkDeviceSize offset, VkDeviceSize size)
{
    if (size == 0)
        return nullptr;

    auto data = stagingBlock.Allocate(size);
    if (data.data == nullptr)
    {
        device.RequestStagingBufferBlock(stagingBlock, size);
        data = stagingBlock.Allocate(size);
    }

    CopyBuffer(buffer, offset, *stagingBlock.cpu, data.offset, size);
    return data.data;
}

void CommandList::CopyToImage(const Image& image, const Buffer& buffer, U32 numBlits, const VkBufferImageCopy* blits)
{
    vkCmdCopyBufferToImage(cmd, buffer.GetBuffer(), image.GetImage(), image.GetImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL), numBlits, blits);
}

void CommandList::CopyBuffer(const Buffer& dst, const Buffer& src)
{
    CopyBuffer(dst, 0, src, 0, src.GetCreateInfo().size);
}

void CommandList::CopyBuffer(const Buffer& dst, VkDeviceSize dstOffset, const Buffer& src, VkDeviceSize srcOffset, VkDeviceSize size)
{
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size = size;
    vkCmdCopyBuffer(cmd, src.GetBuffer(), dst.GetBuffer(), 1, &region);
}

void CommandList::FillBuffer(const BufferPtr& buffer, U32 value)
{
}

void CommandList::SetBindless(U32 set, VkDescriptorSet descriptorSet)
{
    bindlessSets[set] = descriptorSet;
    dirtySets |= 1u << set;
}

void CommandList::SetSampler(U32 set, U32 binding, const Sampler& sampler)
{
    ASSERT(set < VULKAN_NUM_DESCRIPTOR_SETS);
    ASSERT(binding < VULKAN_NUM_BINDINGS);

    if (sampler.GetCookie() == bindings.cookies[set][DESCRIPTOR_SET_TYPE_SAMPLER][binding])
        return;

    ResourceBinding& resBinding = bindings.bindings[set][DESCRIPTOR_SET_TYPE_SAMPLER][binding];
    resBinding.image.sampler = sampler.GetSampler();
    bindings.cookies[set][DESCRIPTOR_SET_TYPE_SAMPLER][binding] = sampler.GetCookie();
    dirtySets |= 1u << set;
}

void CommandList::SetSampler(U32 set, U32 binding, StockSampler type)
{
    ASSERT(set < VULKAN_NUM_DESCRIPTOR_SETS);
    ASSERT(binding < VULKAN_NUM_BINDINGS);

    ImmutableSampler* sampler = device.GetStockSampler(type);
    if (sampler != nullptr)
        SetSampler(set, binding, sampler->GetSampler());
}

void CommandList::SetTexture(U32 set, U32 binding, const ImageView& imageView)
{
    ASSERT(set < VULKAN_NUM_DESCRIPTOR_SETS);
    ASSERT(binding < VULKAN_NUM_BINDINGS);
    ASSERT(imageView.GetImage()->GetCreateInfo().usage & VK_IMAGE_USAGE_SAMPLED_BIT);

    SetTextureImpl(set, binding,
        imageView.GetImageView(), 
        imageView.GetImage()->GetImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
        imageView.GetCookie(),
        DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE);
}

void CommandList::SetStorageTexture(U32 set, U32 binding, const ImageView& view)
{
    ASSERT(view.GetImage()->GetCreateInfo().usage & VK_IMAGE_USAGE_STORAGE_BIT);
    SetTextureImpl(set, binding,
        view.GetImageView(),
        VK_IMAGE_LAYOUT_GENERAL,
        view.GetCookie(),
        DESCRIPTOR_SET_TYPE_STORAGE_IMAGE);
}

void CommandList::SetRasterizerState(const RasterizerState& state)
{
    pipelineState.rasterizerState = state;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetBlendState(const BlendState& state)
{
    pipelineState.blendState = state;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetDepthStencilState(const DepthStencilState& state)
{
    pipelineState.depthStencilState = state;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetPipelineState(const PipelineStateDesc& desc)
{
    const Shader* shaders[static_cast<U32>(ShaderStage::Count)];
    memcpy(shaders, desc.shaders, sizeof(shaders));
    SetProgramShaders(shaders);

    pipelineState.rasterizerState = desc.rasterizerState;
    pipelineState.blendState = desc.blendState;
    pipelineState.depthStencilState = desc.depthStencilState;
    pipelineState.topology = desc.topology;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::SetScissor(const VkRect2D& rect)
{
    ASSERT(rect.offset.x >= 0);
    ASSERT(rect.offset.y >= 0);
    scissor = rect;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_SCISSOR_BIT);
}

void CommandList::SetViewport(const VkViewport& viewport_)
{
    viewport = viewport_;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_VIEWPORT_BIT);
}

void CommandList::SetViewport(const Viewport& viewport_)
{
    viewport.x = viewport_.x;
    viewport.y = viewport_.y + viewport_.height;
    viewport.width = viewport_.width;
    viewport.height = -viewport_.height;    // Flipping the viewport, because of the Y+ of the NDC is down
    viewport.minDepth = viewport_.minDepth;
    viewport.maxDepth = viewport_.maxDepth;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_VIEWPORT_BIT);
}

void CommandList::SetStencilRef(U8 ref, StencilFace face)
{
    switch (face)
    {
    case STENCIL_FACE_FRONT:
        dynamicState.frontReference = ref;
        break;
    case STENCIL_FACE_BACK:
        dynamicState.backReference = ref;
        break;
    case STENCIL_FACE_FRONT_AND_BACK:
        dynamicState.frontReference = ref;
        dynamicState.backReference = ref;
        break;
    default:
        break;
    }

    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_STENCIL_REFERENCE_BIT);
}

void CommandList::NextSubpass(VkSubpassContents contents)
{
    ASSERT(frameBuffer);
    pipelineState.subpassIndex++;
    vkCmdNextSubpass(cmd, contents);
    subpassContents = contents;
    BeginGraphics();
}

void CommandList::Draw(U32 vertexCount, U32 vertexOffset)
{
    ASSERT(!isCompute);
    if (FlushRenderState())
    {
        vkCmdDraw(cmd, vertexCount, 1, vertexOffset, 0);
    }
}

void CommandList::DrawIndexed(U32 indexCount, U32 firstIndex, U32 vertexOffset)
{
    ASSERT(!isCompute);
    ASSERT(indexState.buffer != VK_NULL_HANDLE);
    if (FlushRenderState())
    {
        vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, vertexOffset, 0);
    }
}

void CommandList::DrawIndexedInstanced(U32 indexCount, U32 instanceCount, U32 startIndexLocation, U32 baseVertexLocation, U32 startInstanceLocation)
{
    ASSERT(!isCompute);
    ASSERT(indexState.buffer != VK_NULL_HANDLE);
    if (FlushRenderState())
    {
        vkCmdDrawIndexed(cmd, indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    }
}

void CommandList::Dispatch(U32 groupsX, U32 groupsY, U32 groupsZ)
{
    ASSERT(isCompute);
    if (FlushComputeState())
    {
        vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
    }
}

void CommandList::DispatchIndirect(const Buffer& buffer, U32 offset)
{
    ASSERT(isCompute);
    if (FlushComputeState())
    {
        vkCmdDispatchIndirect(cmd, buffer.GetBuffer(), offset);
    }
}

void CommandList::BufferBarrier(const Buffer& buffer, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    ASSERT(!frameBuffer);

    if (buffer.GetBuffer() == VK_NULL_HANDLE)
        return;

    VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.buffer = buffer.GetBuffer();
    barrier.offset = 0;
    barrier.size = buffer.GetCreateInfo().size;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void CommandList::ImageBarrier(const Image& image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    ASSERT(!frameBuffer);
    ASSERT(image.GetCreateInfo().domain != ImageDomain::Transient);

    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image.GetImage();
    barrier.subresourceRange.aspectMask = formatToAspectMask(image.GetCreateInfo().format);
    barrier.subresourceRange.levelCount = image.GetCreateInfo().levels;
    barrier.subresourceRange.layerCount = image.GetCreateInfo().layers;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandList::Barrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    ASSERT(!frameBuffer);

    VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void CommandList::Barrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, unsigned bufferBarrierCount, const VkBufferMemoryBarrier* bufferBarriers, unsigned imageBarrierCount, const VkImageMemoryBarrier* imageBarriers)
{
    ASSERT(!frameBuffer);

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, bufferBarrierCount, bufferBarriers, imageBarrierCount, imageBarriers);
}

void CommandList::CompleteEvent(const Event& ent)
{
    ASSERT(frameBuffer == nullptr);
    ASSERT(ent.GetStages() != 0);
    vkCmdSetEvent(cmd, ent.GetEvent(), ent.GetStages());
}

void CommandList::WaitEvents(U32 numEvents, VkEvent* events,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
        U32 memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
        U32 bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
        U32 imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    ASSERT(frameBuffer == nullptr);
    ASSERT(srcStageMask != 0);
    ASSERT(dstStageMask != 0);

    vkCmdWaitEvents(
        cmd,
        numEvents, events,
        srcStageMask, dstStageMask,
        memoryBarrierCount, pMemoryBarriers,
        bufferMemoryBarrierCount, pBufferMemoryBarriers,
        imageMemoryBarrierCount, pImageMemoryBarriers
    );
}

void CommandList::BeginEvent(const char* name)
{
    if (device.features.supportDebugUtils)
    {
        VkDebugUtilsLabelEXT info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        info.pLabelName = name;
        info.color[0] = 0.0f;
        info.color[1] = 0.0f;
        info.color[2] = 0.0f;
        info.color[3] = 1.0f;

        if (vkCmdBeginDebugUtilsLabelEXT)
            vkCmdBeginDebugUtilsLabelEXT(cmd, &info);
    }
}

void CommandList::EndEvent()
{
    if (device.features.supportDebugUtils)
    {
        if (vkCmdEndDebugUtilsLabelEXT)
            vkCmdEndDebugUtilsLabelEXT(cmd);
    }
}

void CommandList::EndCommandBufferForThread()
{
    if (isEnded)
        return;

    isEnded = true;

    // Must end a cmd on a same thread we requested it on
    ASSERT(Platform::GetCurrentThreadIndex() == threadIndex);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        Logger::Error("Failed to end command buffer");
}

bool CommandList::FlushRenderState()
{
    if (pipelineState.shaderProgram == nullptr ||
        pipelineState.shaderProgram->IsEmpty())
        return false;

    if (currentPipeline == VK_NULL_HANDLE)
        SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);

    // flush pipeline
    if (IsDirtyAndClear(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT | CommandListDirtyBits::COMMAND_LIST_DIRTY_STATIC_VERTEX_BIT))
    {
        VkPipeline oldPipeline = currentPipeline;
        if (!FlushGraphicsPipeline())
            return false;

        if (oldPipeline != currentPipeline)
        {
            // bind pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline);
            SetDirty(COMMAND_LIST_DIRTY_DYNAMIC_BITS);
        }
    }

    if (currentPipeline == VK_NULL_HANDLE)
        return false;

    // descriptor sets
    FlushDescriptorSets();

    // push constants
    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT))
    {
        auto& range = currentLayout->GetResLayout().pushConstantRange;
        if (range.stageFlags != 0)
        {
            vkCmdPushConstants(cmd, currentPipelineLayout, 
                range.stageFlags, 0, range.size, bindings.pushConstantData);
        }
    }

    // viewport
    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_VIEWPORT_BIT))
    {
        vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    // scissor
    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_SCISSOR_BIT))
    {
        vkCmdSetScissor(cmd, 0, 1, &scissor);
    }

    // stencil reference
    U32 subpassIndex = pipelineState.subpassIndex;
    bool stencilTestEnable = compatibleRenderPass->HasStencil(subpassIndex) && pipelineState.depthStencilState.stencilEnable;
    if (stencilTestEnable && IsDirtyAndClear(COMMAND_LIST_DIRTY_STENCIL_REFERENCE_BIT))
    {
        vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_BIT, dynamicState.frontReference);
        vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_BACK_BIT, dynamicState.backReference);
    }

    // update vbos
    U32 updateVboMask = dirtyVbos & activeVBOs;
    ForEachBitRange(updateVboMask, [&](U32 binding, U32 bindingCount) {
        vkCmdBindVertexBuffers(cmd, binding, bindingCount, vbos.buffers + binding, vbos.offsets + binding);
    });
    dirtyVbos &= ~updateVboMask;

    return true;
}

bool CommandList::FlushComputeState()
{
    if (pipelineState.shaderProgram == nullptr ||
        pipelineState.shaderProgram->IsEmpty())
        return false;

    if (currentPipeline == VK_NULL_HANDLE)
        SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);

    // flush pipeline
    if (IsDirtyAndClear(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT))
    {
        VkPipeline oldPipeline = currentPipeline;
        if (!FlushComputePipeline())
            return false;

        if (oldPipeline != currentPipeline)
        {
            // bind pipeline
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, currentPipeline);
            SetDirty(COMMAND_LIST_DIRTY_DYNAMIC_BITS);
        }
    }

    if (currentPipeline == VK_NULL_HANDLE)
        return false;

    // descriptor sets
    FlushDescriptorSets();

    // push constants
    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT))
    {
        auto& range = currentLayout->GetResLayout().pushConstantRange;
        if (range.stageFlags != 0)
        {
            vkCmdPushConstants(cmd, currentPipelineLayout,
                range.stageFlags, 0, range.size, bindings.pushConstantData);
        }
    }

    return true;
}

bool CommandList::FlushGraphicsPipeline()
{
    if (pipelineState.shaderProgram == nullptr)
        return false;

    if (pipelineState.isOwnedByCommandList)
    {
        UpdateGraphicsPipelineHash(pipelineState, activeVBOs);
        currentPipeline = pipelineState.shaderProgram->GetPipeline(pipelineState.hash);
    }

    if (currentPipeline == VK_NULL_HANDLE)
        currentPipeline = BuildGraphicsPipeline(pipelineState);

    return currentPipeline != VK_NULL_HANDLE;
}

bool CommandList::FlushComputePipeline()
{
    if (pipelineState.shaderProgram == nullptr)
        return false;

    if (pipelineState.isOwnedByCommandList)
    {
        UpdateComputePipelineHash(pipelineState);
        currentPipeline = pipelineState.shaderProgram->GetPipeline(pipelineState.hash);
    }
    
    if (currentPipeline == VK_NULL_HANDLE)
        currentPipeline = BuildComputePipeline(pipelineState);

    return currentPipeline != VK_NULL_HANDLE;
}

void CommandList::FlushDescriptorSets()
{
    auto& resLayout = currentLayout->GetResLayout();

    // find all set need to update
    U32 setUpdate = resLayout.descriptorSetMask & dirtySets;
    ForEachBit(setUpdate, [&](U32 set) {
        FlushDescriptorSet(set);
    });
    dirtySets &= ~setUpdate;

    // Update for the dynamic offset of constant buffer
    dirtySetsDynamic &= ~setUpdate;
    U32 dynamicSetUpdate = resLayout.descriptorSetMask & dirtySetsDynamic;
    ForEachBit(dynamicSetUpdate, [&](U32 set) {
        FlushDescriptorDynamicSet(set);
    });
    dirtySetsDynamic &= ~dynamicSetUpdate;
}

void CommandList::FlushDescriptorSet(U32 set)
{
    auto& resLayout = currentLayout->GetResLayout();
    // check is bindless descriptor set
    if (resLayout.bindlessSetMask & (1u << set))
    {
        vkCmdBindDescriptorSets(cmd, 
            renderPass ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE,
            currentPipelineLayout, 
            set, 
            1, 
            &bindlessSets[set], 
            0,
            nullptr);
        return;
    }

    // allocator new descriptor set
    DescriptorSetAllocator* allocator = currentLayout->GetAllocator(set);
    if (allocator == nullptr)
        return;
    
    const DescriptorSetLayout& setLayout = resLayout.sets[set];

    uint32_t numDynamicOffsets = 0;
    uint32_t dynamicOffsets[VULKAN_NUM_BINDINGS];
    std::vector<ResourceBinding> resourceBindings;

    // Calculate descriptor set layout hash
    HashCombiner hasher;
    // Sampled images
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE][binding + i]);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_SAMPLED_IMAGE][binding + i].image.imageLayout);
        }
    });
    // Storage images
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_STORAGE_IMAGE], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_STORAGE_IMAGE][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_STORAGE_IMAGE][binding + i]);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_STORAGE_IMAGE][binding + i].image.imageLayout);
        }
    });
    // UBOs
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER], [&](U32 binding) 
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding + i]);
            auto& b = bindings.bindings[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding + i];
            hasher.HashCombine(b.buffer.range);
            ASSERT(b.buffer.buffer != VK_NULL_HANDLE);
            dynamicOffsets[numDynamicOffsets++] = b.dynamicOffset;
        }
    });
    // Storage buffers
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_STORAGE_BUFFER], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_STORAGE_BUFFER][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_STORAGE_BUFFER][binding + i]);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_STORAGE_BUFFER][binding + i].buffer.offset);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_STORAGE_BUFFER][binding + i].buffer.range);
        }
    });
    // Sampled buffers
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER][binding]; i++)
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER][binding + i]);
    });
    // Input attachments
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT][binding + i]);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_INPUT_ATTACHMENT][binding + i].image.imageLayout);
        }
    });
    // Samplers
    ForEachBit(setLayout.masks[DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER], [&](U32 binding)
    {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER][binding]; i++)
        {
            hasher.HashCombine(bindings.cookies[set][DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER][binding + i]);
            hasher.HashCombine(bindings.bindings[set][DESCRIPTOR_SET_TYPE_SAMPLED_BUFFER][binding + i].image.imageLayout);
        }
    });

    // Get all resource bindings
    for (U32 maskbit = 0; maskbit < DESCRIPTOR_SET_TYPE_COUNT; maskbit++)
    {
        ForEachBit(setLayout.masks[maskbit], [&](U32 binding) {
            for (U8 i = 0; i < setLayout.arraySize[maskbit][binding]; i++)
                resourceBindings.push_back(bindings.bindings[set][maskbit][binding + i]);
        });
    }

    auto allocated = allocator->GetOrAllocate(threadIndex, hasher.Get());
	if (!allocated.second) 
    {
        auto updateTemplate = currentLayout->GetUpdateTemplate(set);
        ASSERT(updateTemplate);
        vkUpdateDescriptorSetWithTemplate(device.device, allocated.first, updateTemplate, resourceBindings.data());
    }

    vkCmdBindDescriptorSets(
        cmd, 
        renderPass ? VK_PIPELINE_BIND_POINT_GRAPHICS : VK_PIPELINE_BIND_POINT_COMPUTE,
        currentPipelineLayout,
        set, 
        1, 
        &allocated.first,
        numDynamicOffsets,
        dynamicOffsets);
    allocatedSets[set] = allocated.first;
}

void CommandList::FlushDescriptorDynamicSet(U32 set)
{
    auto& resLayout = currentLayout->GetResLayout();
    // check is bindless descriptor set
    if (resLayout.bindlessSetMask & (1u << set))
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            currentPipelineLayout, set, 1, &bindlessSets[set], 0, nullptr);
        return;
    }

    // Update constant buffers
    U32 numDynamicOffsets = 0;
    U32 dynamicOffsets[VULKAN_NUM_BINDINGS];

    const DescriptorSetLayout& setLayout = resLayout.sets[set];
    const U32 uniformMask = setLayout.masks[DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER];
    ForEachBit(uniformMask, [&](U32 binding) {
        for (U8 i = 0; i < setLayout.arraySize[DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding]; i++) {
            dynamicOffsets[numDynamicOffsets++] = bindings.bindings[set][DESCRIPTOR_SET_TYPE_UNIFORM_BUFFER][binding + i].dynamicOffset;
        }
    });

    ASSERT(allocatedSets[set] != VK_NULL_HANDLE);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        currentPipelineLayout, set, 1, &allocatedSets[set], numDynamicOffsets, dynamicOffsets);
}

VkPipeline CommandList::BuildGraphicsPipeline(const CompiledPipelineState& pipelineState)
{
    U32 subpassIndex = pipelineState.subpassIndex;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // dynamic state
    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = 2;
    VkDynamicState states[7] = {
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_VIEWPORT,
    };

    bool stencilTestEnable = compatibleRenderPass->HasStencil(subpassIndex) && pipelineState.depthStencilState.stencilEnable;
    if (stencilTestEnable)
    {
        states[dynamicState.dynamicStateCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }

    dynamicState.pDynamicStates = states;

    // blend state
    VkPipelineColorBlendAttachmentState blendAttachments[VULKAN_NUM_ATTACHMENTS];
    int attachmentCount = compatibleRenderPass->GetNumColorAttachments(subpassIndex);
    for (int i = 0; i < attachmentCount; i++)
    {
        VkPipelineColorBlendAttachmentState& attachment = blendAttachments[i];
        attachment = {};

        if (compatibleRenderPass->GetColorAttachment(subpassIndex, i).attachment == VK_ATTACHMENT_UNUSED)
            continue;

        const auto& desc = pipelineState.blendState.renderTarget[i];
        attachment.colorWriteMask = 0;
        if (desc.renderTargetWriteMask & COLOR_WRITE_ENABLE_RED)
            attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
        if (desc.renderTargetWriteMask & COLOR_WRITE_ENABLE_GREEN)
            attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
        if (desc.renderTargetWriteMask & COLOR_WRITE_ENABLE_BLUE)
            attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
        if (desc.renderTargetWriteMask & COLOR_WRITE_ENABLE_ALPHA)
            attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

        attachment.blendEnable = desc.blendEnable ? VK_TRUE : VK_FALSE;
        if (attachment.blendEnable)
        {
            attachment.srcColorBlendFactor = desc.srcBlend;
            attachment.dstColorBlendFactor = desc.destBlend;
            attachment.colorBlendOp = desc.blendOp;
            attachment.srcAlphaBlendFactor = desc.srcBlendAlpha;
            attachment.dstAlphaBlendFactor = desc.destBlendAlpha;
            attachment.alphaBlendOp = desc.blendOpAlpha;
        }
    }
    VkPipelineColorBlendStateCreateInfo colorBlending = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.pAttachments = blendAttachments;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = blendAttachments;
    colorBlending.blendConstants[0] = 1.0f;
    colorBlending.blendConstants[1] = 1.0f;
    colorBlending.blendConstants[2] = 1.0f;
    colorBlending.blendConstants[3] = 1.0f;

    // depth state
    VkPipelineDepthStencilStateCreateInfo depthstencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthstencil.depthTestEnable = compatibleRenderPass->HasDepth(subpassIndex) && pipelineState.depthStencilState.depthEnable ? VK_TRUE : VK_FALSE;
    depthstencil.depthWriteEnable = compatibleRenderPass->HasDepth(subpassIndex) && pipelineState.depthStencilState.depthWriteMask != DEPTH_WRITE_MASK_ZERO ? VK_TRUE : VK_FALSE;
    depthstencil.stencilTestEnable = compatibleRenderPass->HasStencil(subpassIndex) && pipelineState.depthStencilState.stencilEnable ? VK_TRUE : VK_FALSE;

    if (depthstencil.depthTestEnable)
        depthstencil.depthCompareOp = pipelineState.depthStencilState.depthFunc;

    if (depthstencil.stencilTestEnable)
    {
        depthstencil.front.compareMask = pipelineState.depthStencilState.stencilReadMask;
        depthstencil.front.writeMask = pipelineState.depthStencilState.stencilWriteMask;
        depthstencil.front.reference = 0; // runtime supplied
        depthstencil.front.compareOp = pipelineState.depthStencilState.frontFace.stencilFunc;
        depthstencil.front.passOp = pipelineState.depthStencilState.frontFace.stencilPassOp;
        depthstencil.front.failOp = pipelineState.depthStencilState.frontFace.stencilFailOp;
        depthstencil.front.depthFailOp = pipelineState.depthStencilState.frontFace.stencilDepthFailOp;

        depthstencil.back.compareMask = pipelineState.depthStencilState.stencilReadMask;
        depthstencil.back.writeMask = pipelineState.depthStencilState.stencilWriteMask;
        depthstencil.back.reference = 0; // runtime supplied
        depthstencil.back.compareOp = pipelineState.depthStencilState.backFace.stencilFunc;
        depthstencil.back.passOp = pipelineState.depthStencilState.backFace.stencilPassOp;
        depthstencil.back.failOp = pipelineState.depthStencilState.backFace.stencilFailOp;
        depthstencil.back.depthFailOp = pipelineState.depthStencilState.backFace.stencilDepthFailOp;
    }
    depthstencil.depthBoundsTestEnable = VK_FALSE;

    // vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    // attributes
    U32 numAttributes = 0;
    VkVertexInputAttributeDescription attributes[VULKAN_NUM_VERTEX_ATTRIBS];
    U32 bindingMask = 0;
    U32 attributeMask = pipelineState.shaderProgram->GetPipelineLayout()->GetResLayout().attributeInputMask;
    ForEachBit(attributeMask, [&](U32 attributeIndex){
        VkVertexInputAttributeDescription& attr = attributes[numAttributes++];
        attr.location = attributeIndex;
        attr.binding = pipelineState.attribs[attributeIndex].binding;
        attr.format = pipelineState.attribs[attributeIndex].format;
        attr.offset = pipelineState.attribs[attributeIndex].offset;

        bindingMask |= 1u << attr.binding;
    });

    // bindings
    U32 numBindings = 0;
    VkVertexInputBindingDescription bindings[VULKAN_NUM_VERTEX_BUFFERS];
    ForEachBit(bindingMask, [&](U32 binding) {
        VkVertexInputBindingDescription& bind = bindings[numBindings++];
        bind.binding = binding;
        bind.inputRate = vbos.inputRate[binding];
        bind.stride = (U32)vbos.strides[binding];
     });

    vertexInputInfo.vertexBindingDescriptionCount = numBindings;
    vertexInputInfo.pVertexBindingDescriptions = bindings;
    vertexInputInfo.vertexAttributeDescriptionCount = numAttributes;
    vertexInputInfo.pVertexAttributeDescriptions = attributes;

    // input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = pipelineState.topology;

    // raster
    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = pipelineState.rasterizerState.depthClipEnable;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = pipelineState.rasterizerState.fillMode == FILL_SOLID ? VK_POLYGON_MODE_FILL : VK_POLYGON_MODE_LINE;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = pipelineState.rasterizerState.cullMode;
    rasterizer.frontFace = pipelineState.rasterizerState.frontCounterClockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    // MSAA
    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // stages
    VkPipelineShaderStageCreateInfo stages[static_cast<unsigned>(ShaderStage::Count)];
    int stageCount = 0;

    for (int i = 0; i < static_cast<unsigned>(ShaderStage::Count); i++)
    {
        ShaderStage shaderStage = static_cast<ShaderStage>(i);
        if (pipelineState.shaderProgram->GetShader(shaderStage))
        {
            const Shader* shader = pipelineState.shaderProgram->GetShader(shaderStage);

            VkPipelineShaderStageCreateInfo& stageCreateInfo = stages[stageCount++];
            stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            stageCreateInfo.pName = shader->GetEntryPoint();
            stageCreateInfo.module = shader->GetModule();

            switch (shaderStage)
            {
            case ShaderStage::MS:
                stageCreateInfo.stage = VK_SHADER_STAGE_MESH_BIT_NV;
                break;
            case ShaderStage::AS:
                stageCreateInfo.stage = VK_SHADER_STAGE_TASK_BIT_NV;
                break;
            case ShaderStage::VS:
                stageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case ShaderStage::HS:
                stageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                break;
            case ShaderStage::DS:
                stageCreateInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                break;
            case ShaderStage::GS:
                stageCreateInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                break;
            case ShaderStage::PS:
                stageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            case ShaderStage::CS:
                stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
                break;
            default:
                stageCreateInfo.stage = VK_SHADER_STAGE_ALL;
                break;
            }
        }
    }

    // Create pipeline
    VkPipeline retPipeline = VK_NULL_HANDLE;
    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.layout = pipelineState.shaderProgram->GetPipelineLayout()->GetLayout();
    pipelineInfo.renderPass = compatibleRenderPass->GetRenderPass();
    pipelineInfo.subpass = pipelineState.subpassIndex;

    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &depthstencil;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pStages = stages;
    pipelineInfo.stageCount = stageCount;

    VkResult res = vkCreateGraphicsPipelines(device.device, pipelineState.cache, 1, &pipelineInfo, nullptr, &retPipeline);
    if (res != VK_SUCCESS)
    {
        Logger::Error("Failed to create graphics pipeline!");
        return VK_NULL_HANDLE;
    }

    // Handle pipeline by shader program
    pipelineState.shaderProgram->AddPipeline(pipelineState.hash, retPipeline);

    return retPipeline;
}

VkPipeline CommandList::BuildComputePipeline(const CompiledPipelineState& pipelineState)
{
    const Shader* shader = pipelineState.shaderProgram->GetShader(ShaderStage::CS);
    ASSERT(shader != nullptr);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.layout = pipelineState.shaderProgram->GetPipelineLayout()->GetLayout();
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.module = shader->GetModule();
    info.stage.pName = shader->GetEntryPoint();
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkResult ret = vkCreateComputePipelines(
        device.device,
        pipelineState.cache, 
        1, 
        &info, 
        nullptr, 
        &computePipeline
    );
    if (ret != VK_SUCCESS || computePipeline == VK_NULL_HANDLE)
    {
        if (ret < 0)
            Logger::Error("Failed to create compute pipeline!");
        return {};
    }

    // Handle pipeline by shader program
    pipelineState.shaderProgram->AddPipeline(pipelineState.hash, computePipeline);

    return computePipeline;
}

void CommandList::BeginCompute()
{
    isCompute = true;
    ResetCommandContext();
}

void CommandList::BeginGraphics()
{
    isCompute = false;
    ResetCommandContext();
}

void CommandList::UpdateGraphicsPipelineHash(CompiledPipelineState& pipeline, U32& activeVbos)
{
    ASSERT(pipeline.shaderProgram != nullptr);
    HashCombiner hash;
    activeVbos = 0;
    const CombinedResourceLayout& layout = pipeline.shaderProgram->GetPipelineLayout()->GetResLayout();
  
    ForEachBit(layout.attributeInputMask, [&](U32 bit) {
        hash.HashCombine(bit);
        hash.HashCombine(pipeline.attribs[bit].binding);
        hash.HashCombine(pipeline.attribs[bit].format);
        hash.HashCombine(pipeline.attribs[bit].offset);

        activeVbos |= 1u << pipeline.attribs[bit].binding;
    });

    ForEachBit(activeVbos, [&](U32 bit) {
        hash.HashCombine(vbos.inputRate[bit]);
        hash.HashCombine(vbos.strides[bit]);
    });

    hash.HashCombine(compatibleRenderPass->GetHash());
    hash.HashCombine(pipeline.subpassIndex);
    hash.HashCombine(pipeline.shaderProgram->GetHash());

    // Calculate pipelinestate hash
    hash.HashCombine(RuntimeHash(&pipeline.blendState, sizeof(BlendState)).GetHashValue());
    hash.HashCombine(RuntimeHash(&pipeline.rasterizerState, sizeof(RasterizerState)).GetHashValue());
    hash.HashCombine(RuntimeHash(&pipeline.depthStencilState, sizeof(DepthStencilState)).GetHashValue());
    hash.HashCombine(pipeline.topology);

    pipeline.hash = hash.Get();
}

void CommandList::UpdateComputePipelineHash(CompiledPipelineState& pipeline)
{
    HashCombiner hash;
    hash.HashCombine(pipeline.shaderProgram->GetHash());
    pipeline.hash = hash.Get();
}

}
}