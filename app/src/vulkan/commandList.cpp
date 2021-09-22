#include "commandList.h"
#include "vulkan/device.h"

CommandPool::CommandPool(DeviceVulkan* device_, uint32_t queueFamilyIndex) :
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
        vkFreeCommandBuffers(device->device, pool, (uint32_t)buffers.size(), buffers.data());

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
            vkFreeCommandBuffers(device->device, pool, (uint32_t)buffers.size(), buffers.data());
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
    frameBuffer = &device.RequestFrameBuffer(renderPassInfo);
    compatibleRenderPass = &frameBuffer->GetRenderPass();
    renderPass = &device.RequestRenderPass(renderPassInfo);

    // area
    VkRect2D rect = {};
    uint32_t fbWidth = frameBuffer->GetWidth();
    uint32_t fbHeight = frameBuffer->GetHeight();
    rect.offset.x = min(fbWidth, uint32_t(rect.offset.x));
    rect.offset.y = min(fbHeight, uint32_t(rect.offset.y));
    rect.extent.width = min(fbWidth - rect.offset.x, rect.extent.width);
    rect.extent.height = min(fbHeight - rect.offset.y, rect.extent.height);

    viewport = {
        float(rect.offset.x), float(rect.offset.y),
        float(rect.extent.width), float(rect.extent.height),
        0.0f, 1.0f
    };
    scissor = rect;

    // clear color
    // 遍历所有colorAttachments同时检查clearAttachments mask
    VkClearValue clearColors[VULKAN_NUM_ATTACHMENTS + 1];
    uint32_t numClearColor = 0;
    for (uint32_t i = 0; i < renderPassInfo.numColorAttachments; i++)
    {
        if (renderPassInfo.clearAttachments & (1u << i))
        {
            clearColors[i].color = {0.0f, 0.0f, 0.0f, 1.0f};
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
        clearColors[renderPassInfo.numColorAttachments].depthStencil = {1.0f, 0};
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
}

// 清除除Program意以外的管线状态
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
	bd.renderTarget[0].destBlendAlpha = VK_BLEND_FACTOR_ZERO    ;
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
	rs.frontCounterClockwise = true;
	rs.depthBias = 0;
	rs.depthBiasClamp = 0;
	rs.slopeScaledDepthBias = 0;
	rs.depthClipEnable = true;
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
    SetDirty(COMMAND_LIST_DIRTY_PIPELINE_BIT | COMMAND_LIST_DIRTY_DYNAMIC_BITS);
    currentPipeline = VK_NULL_HANDLE;

    if (program == nullptr)
        return;

    if (currentLayout == nullptr)
    {
        currentLayout = program->GetPipelineLayout();
        currentPipelineLayout = currentLayout->GetLayout();
    }
    else if (program->GetPipelineLayout()->GetHash() != currentLayout->GetHash())
    {
        currentLayout = program->GetPipelineLayout();
        currentPipelineLayout = currentLayout->GetLayout();
    }
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

void CommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex, uint32_t vertexOffset)
{
    if (FlushRenderState())
    {
        vkCmdDrawIndexed(cmd, indexCount, 1, firstIndex, vertexOffset, 0);
    }
}

bool CommandList::FlushRenderState()
{
    if (pipelineState.shaderProgram == nullptr || 
        pipelineState.shaderProgram->IsEmpty())
        return false;

    if (currentPipeline == VK_NULL_HANDLE)
        SetDirty(CommandListDirtyBits::COMMAND_LIST_DIRTY_PIPELINE_BIT);

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

    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_VIEWPORT_BIT))
    {
        vkCmdSetViewport(cmd, 0, 1, &viewport);
    }

    if (IsDirtyAndClear(COMMAND_LIST_DIRTY_VIEWPORT_BIT))
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
        device.UpdateGraphicsPipelineHash(pipelineState);
        currentPipeline = pipelineState.shaderProgram->GetPipeline(pipelineState.hash);
    }

    if (currentPipeline == VK_NULL_HANDLE)
        currentPipeline = BuildGraphicsPipeline(pipelineState);

    return currentPipeline != VK_NULL_HANDLE;
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
    VkVertexInputAttributeDescription& attr = attributes.emplace_back();
    attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attr.location = 0;
    attr.offset = 0;

    // bindings
    VkVertexInputBindingDescription& bind = bindings.emplace_back();
    bind.binding = 0;
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bind.stride = 0;

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
    vertexInputInfo.pVertexBindingDescriptions = bindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
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
    rasterizer.frontFace = pipelineState.rasterizerState.frontCounterClockwise ?  VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    return retPipeline;
}

VkPipeline CommandList::BuildComputePipeline(const CompilePipelineState& pipelineState)
{
    return VkPipeline();
}
