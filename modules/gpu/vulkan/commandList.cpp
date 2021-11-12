﻿#include "commandList.h"
#include "vulkan/device.h"

namespace GPU
{
namespace 
{
    static void UpdateDescriptorSet(DeviceVulkan &device, 
                                    VkDescriptorSet descSet,
                                    const DescriptorSetLayout &setLayout, 
                                    const ResourceBinding *bindings)
    {
        // update descriptor sets according setLayout masks
        U32 writeCount = 0;
        VkWriteDescriptorSet writes[VULKAN_NUM_BINDINGS];

        // storage image
        ForEachBit(setLayout.masks[static_cast<U32>(DescriptorSetLayout::STORAGE_IMAGE)], 
            [&](U32 binding) {
                U32 arraySize = setLayout.arraySize[binding];
                for(U32 i = 0; i < arraySize; i++)
                {
                    auto& write = writes[writeCount++];
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.pNext = nullptr;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    write.dstArrayElement = i;
                    write.dstBinding = binding;
                    write.dstSet = descSet;
                    write.pImageInfo = &bindings[binding + i].image;
                }
            });

        // input attachment
        ForEachBit(setLayout.masks[static_cast<U32>(DescriptorSetLayout::INPUT_ATTACHMENT)], 
            [&](U32 binding) {
                U32 arraySize = setLayout.arraySize[binding];
                for(U32 i = 0; i < arraySize; i++)
                {
                    auto& write = writes[writeCount++];
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.pNext = nullptr;
                    write.descriptorCount = 1;
                    write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                    write.dstArrayElement = i;
                    write.dstBinding = binding;
                    write.dstSet = descSet;
                    write.pImageInfo = &bindings[binding + i].image;
                }
            });

        vkUpdateDescriptorSets(device.device, writeCount, writes, 0, nullptr);
    }
}

CommandPool::CommandPool(DeviceVulkan* device_, U32 queueFamilyIndex) :
    device(device_)
{
    VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = queueFamilyIndex;

    if (queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        VkResult res = vkCreateCommandPool(device->device, &info, nullptr, &pool);
        assert(res == VK_SUCCESS);
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
        usedIndex = 0;

        std::swap(pool, other.pool);
        std::swap(buffers, other.buffers);
        std::swap(usedIndex, other.usedIndex);
    }

    return *this;
}

VkCommandBuffer CommandPool::RequestCommandBuffer()
{
    if (usedIndex < buffers.size())
    {
        return buffers[usedIndex++];
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    info.commandPool = pool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(device->device, &info, &cmd);
    assert(res == VK_SUCCESS);

    buffers.push_back(cmd);
    usedIndex++;

    return cmd;
}

void CommandPool::BeginFrame()
{
    if (usedIndex > 0)
    {
        usedIndex = 0;
        vkResetCommandPool(device->device, pool, 0);
    }
}

void CommandListDeleter::operator()(CommandList* cmd)
{
    if (cmd != nullptr)
        cmd->device.commandListPool.free(cmd);
}

CommandList::CommandList(DeviceVulkan& device_, VkCommandBuffer buffer, QueueType type) :
    device(device_),
    cmd(buffer),
    type(type)
{
}

CommandList::~CommandList()
{
}

void CommandList::BeginRenderPass(const RenderPassInfo& renderPassInfo)
{
    assert(frameBuffer == nullptr);
    assert(compatibleRenderPass == nullptr);

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
        float(rect.extent.width), float(rect.extent.height),
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
            clearColors[i].color = { 0.0f, 0.0f, 0.0f, 1.0f };
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
        clearColors[renderPassInfo.numColorAttachments].depthStencil = { 1.0f, 0 };
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

    // begin graphics contexxt
    BeginGraphicsContext();
}

void CommandList::EndRenderPass()
{
    vkCmdEndRenderPass(cmd);

    // clear runtime resources
    frameBuffer = nullptr;
    renderPass = nullptr;
    compatibleRenderPass = nullptr;
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
    dsd.depthEnable = false;
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

void CommandList::SetProgram(ShaderProgram* program)
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
   auto* tempProgram = device.GetShaderManager().RegisterGraphics(vertex, fragment, defines);
   if (tempProgram == nullptr)
   {
       SetProgram(nullptr);
       return;
   }

   auto variant = tempProgram->RegisterVariant(defines);
   SetProgram(variant->GetProgram());
}

void CommandList::BeginGraphicsContext()
{
    dirty = ~0u;  // set all things are dirty
    pipelineState.shaderProgram = nullptr;
    currentPipeline = VK_NULL_HANDLE;
    currentPipelineLayout = VK_NULL_HANDLE;
    currentLayout = nullptr;
}

void CommandList::EndCommandBuffer()
{
    VkResult res = vkEndCommandBuffer(cmd);
    assert(res == VK_SUCCESS);
}

void CommandList::BindPipelineState(const CompilePipelineState& pipelineState_)
{
    SetProgram(pipelineState_.shaderProgram);
    pipelineState = pipelineState_;
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);
}

void CommandList::BindVertexBuffers()
{
}

void CommandList::BindIndexBuffers()
{
}

void CommandList::PushConstants(const void* data, VkDeviceSize offset, VkDeviceSize range)
{
    assert(offset + range <= VULKAN_PUSH_CONSTANT_SIZE);
    memcpy(bindings.pushConstantData + offset, data, range);
    SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PUSH_CONSTANTS_BIT);
}

void CommandList::CopyToImage(const ImagePtr& image, const BufferPtr& buffer, U32 numBlits, const VkBufferImageCopy* blits)
{
}

void CommandList::Draw(U32 vertexCount, U32 vertexOffset)
{
    if (FlushRenderState())
    {
        vkCmdDraw(cmd, vertexCount, 1, vertexOffset, 0);
    }
}

void CommandList::DrawIndexed(U32 indexCount, U32 firstIndex, U32 vertexOffset)
{
    if (FlushRenderState())
    {
        vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, vertexOffset, 0);
    }
}

void CommandList::ImageBarrier(const ImagePtr& image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.image = image->GetImage();
    barrier.subresourceRange.aspectMask = formatToAspectMask(image->GetCreateInfo().format);
    barrier.subresourceRange.levelCount = image->GetCreateInfo().levels;
    barrier.subresourceRange.layerCount = image->GetCreateInfo().layers;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CommandList::Barrier(VkPipelineStageFlags srcStage, VkAccessFlags srcAccess, VkPipelineStageFlags dstStage, VkAccessFlags dstAccess)
{
    VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void CommandList::Barrier(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, unsigned imageBarrierCount, const VkImageMemoryBarrier* imageBarriers)
{
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, imageBarrierCount, imageBarriers);
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

bool CommandList::FlushRenderState()
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

    return true;
}

bool CommandList::FlushGraphicsPipeline()
{
    if (pipelineState.shaderProgram == nullptr)
        return false;

    if (pipelineState.isOwnedByCommandList)
    {
        UpdateGraphicsPipelineHash(pipelineState);
        currentPipeline = pipelineState.shaderProgram->GetPipeline(pipelineState.hash);
    }

    if (currentPipeline == VK_NULL_HANDLE)
        currentPipeline = BuildGraphicsPipeline(pipelineState);

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

}

void CommandList::FlushDescriptorSet(U32 set)
{
    auto& resLayout = currentLayout->GetResLayout();
    // check is bindless descriptor set
    if (false)
    {
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
            currentPipelineLayout, set, 1, &bindlessSets[set], 0, nullptr);
        return;
    }

    // allocator new descriptor set
    auto allocator = currentLayout->GetAllocator(set);
    if (allocator == nullptr)
        return;
    
    HashCombiner hasher;
    auto allocated = allocator->GetOrAllocate(hasher.Get());
    // The descriptor set was not successfully cached, rebuild.
	if (!allocated.second)
	{
        UpdateDescriptorSet(device, allocated.first, resLayout.sets[set], bindings.bindings[set]);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
        currentPipelineLayout, 0, 1, &allocated.first, 0, nullptr);
    allocatedSets[set] = allocated.first;
}

VkPipeline CommandList::BuildGraphicsPipeline(const CompilePipelineState& pipelineState)
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
            attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            attachment.colorBlendOp = VK_BLEND_OP_MAX;
            attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            attachment.alphaBlendOp = VK_BLEND_OP_ADD;
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
        depthstencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
        depthstencil.front.passOp = VK_STENCIL_OP_REPLACE;
        depthstencil.front.failOp = VK_STENCIL_OP_KEEP;
        depthstencil.front.depthFailOp = VK_STENCIL_OP_KEEP;

        depthstencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depthstencil.back.passOp = VK_STENCIL_OP_REPLACE;
        depthstencil.back.failOp = VK_STENCIL_OP_KEEP;
        depthstencil.back.depthFailOp = VK_STENCIL_OP_KEEP;
    }
    depthstencil.depthBoundsTestEnable = VK_FALSE;

    // vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    // attributes
    U32 bindingMask = 0;
    U32 attributeMask = pipelineState.shaderProgram->GetPipelineLayout()->GetResLayout().attributeInputMask;
    ForEachBit(attributeMask, [&](U32 attributeIndex){
        VkVertexInputAttributeDescription& attr = attributes.emplace_back();
        attr.location = attributeIndex;
        attr.format = pipelineState.attribs[attributeIndex].format;
        attr.offset = pipelineState.attribs[attributeIndex].offset;

        bindingMask |= pipelineState.attribs[attributeIndex].binding;
    });

    // bindings
    ForEachBit(bindingMask, [&](U32 binding) {
        VkVertexInputBindingDescription& bind = bindings.emplace_back();
        bind.binding = binding;
        bind.inputRate = pipelineState.inputRates[binding];
        bind.stride = (U32)pipelineState.strides[binding];
     });

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<U32>(bindings.size());
    vertexInputInfo.pVertexBindingDescriptions = bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<U32>(attributes.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributes.data();

    // input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    inputAssemblyInfo.topology = pipelineState.topology;

    // raster
    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_TRUE;
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
            VkPipelineShaderStageCreateInfo& stageCreateInfo = stages[stageCount++];
            stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            stageCreateInfo.pName = "main";
            stageCreateInfo.module = pipelineState.shaderProgram->GetShader(shaderStage)->GetModule();

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

VkPipeline CommandList::BuildComputePipeline(const CompilePipelineState& pipelineState)
{
    return VkPipeline();
}

void CommandList::UpdateGraphicsPipelineHash(CompilePipelineState pipeline)
{
    assert(pipeline.shaderProgram != nullptr);
    HashCombiner hash;
    const CombinedResourceLayout& layout = pipeline.shaderProgram->GetPipelineLayout()->GetResLayout();
    ForEachBit(layout.attributeInputMask, [&](U32 bit) {
        hash.HashCombine(bit);
        hash.HashCombine(pipeline.attribs[bit].binding);
        hash.HashCombine(pipeline.attribs[bit].format);
        hash.HashCombine(pipeline.attribs[bit].offset);
    });

    hash.HashCombine(compatibleRenderPass->GetHash());
    hash.HashCombine(pipeline.subpassIndex);
    hash.HashCombine(pipeline.shaderProgram->GetHash());

    pipeline.hash = hash.Get();
}

}