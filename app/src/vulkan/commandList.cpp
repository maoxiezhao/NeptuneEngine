#include "commandList.h"
#include "gpu.h"

CommandPool::CommandPool(DeviceVulkan* device, uint32_t queueFamilyIndex) :
    mDevice(device)
{
    VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = queueFamilyIndex;

    if (queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED)
    {
        VkResult res = vkCreateCommandPool(device->mDevice, &info, nullptr, &mPool);
        assert(res == VK_SUCCESS);
    }
}

CommandPool::~CommandPool()
{
    if (!mBuffers.empty())
        vkFreeCommandBuffers(mDevice->mDevice, mPool, (uint32_t)mBuffers.size(), mBuffers.data());

    if (mPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(mDevice->mDevice, mPool, nullptr);
}

CommandPool::CommandPool(CommandPool&& other) noexcept
{
    *this = std::move(other);
}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept
{
    if (this != &other)
    {
        mDevice = other.mDevice;
        if (!mBuffers.empty())
            vkFreeCommandBuffers(mDevice->mDevice, mPool, (uint32_t)mBuffers.size(), mBuffers.data());
        if (mPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(mDevice->mDevice, mPool, nullptr);

        mPool = VK_NULL_HANDLE;
        mBuffers.clear();
        mUsedIndex = 0;

        std::swap(mPool, other.mPool);
        std::swap(mBuffers, other.mBuffers);
        std::swap(mUsedIndex, other.mUsedIndex);
    }

    return *this;
}

VkCommandBuffer CommandPool::RequestCommandBuffer()
{
    if (mUsedIndex < mBuffers.size())
    {
        return mBuffers[mUsedIndex++];
    }

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    info.commandPool = mPool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(mDevice->mDevice, &info, &cmd);
    assert(res == VK_SUCCESS);

    mBuffers.push_back(cmd);
    mUsedIndex++;

    return cmd;
}

void CommandPool::BeginFrame()
{
    if (mUsedIndex > 0)
    {
        mUsedIndex = 0;
        vkResetCommandPool(mDevice->mDevice, mPool, 0);
    }
}

void CommandListDeleter::operator()(CommandList* cmd)
{
    if (cmd != nullptr)
        cmd->mDevice.mCommandListPool.free(cmd);
}

CommandList::CommandList(DeviceVulkan& device, VkCommandBuffer buffer, QueueType type) :
    mDevice(device),
    mCmd(buffer),
    mType(type)
{
}

CommandList::~CommandList()
{
    if (mCurrentPipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(mDevice.mDevice, mCurrentPipeline, nullptr);
}

void CommandList::BeginRenderPass(const RenderPassInfo& renderPassInfo)
{
    mFrameBuffer = &mDevice.RequestFrameBuffer(renderPassInfo);
    mRenderPass = &mDevice.RequestRenderPass(renderPassInfo);

    // area
    VkRect2D scissor = {};

    // clear color
    VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

    // begin rende pass
    VkRenderPassBeginInfo beginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    beginInfo.renderArea = scissor;
    beginInfo.renderPass = mRenderPass->mRenderPass;
    beginInfo.framebuffer = mFrameBuffer->mFrameBuffer;
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(mCmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void CommandList::EndRenderPass()
{
    vkCmdEndRenderPass(mCmd);
}

void CommandList::EndCommandBuffer()
{
    VkResult res = vkEndCommandBuffer(mCmd);
    assert(res == VK_SUCCESS);
}

bool CommandList::FlushRenderState()
{
    if (mCurrentPipeline == VK_NULL_HANDLE)
        mIsDirtyPipeline = true;

    if (mIsDirtyPipeline)
    {
        mIsDirtyPipeline = false;
        VkPipeline oldPipeline = mCurrentPipeline;
        if (!FlushGraphicsPipeline())
            return false;

        if (oldPipeline != mCurrentPipeline)
        {
            // bind pipeline
            vkCmdBindPipeline(mCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mCurrentPipeline);
        }
    }

    return true;
}

bool CommandList::FlushGraphicsPipeline()
{
    mCurrentPipeline = BuildGraphicsPipeline(mPipelineStateDesc);
    return mCurrentPipeline != VK_NULL_HANDLE;
}

VkPipeline CommandList::BuildGraphicsPipeline(const PipelineStateDesc& pipelineStateDesc)
{
    /////////////////////////////////////////////////////////////////////////////////////////
    // view port
    mViewport.x = 0;
    mViewport.y = 0;
    mViewport.width = 800;
    mViewport.height = 600;
    mViewport.minDepth = 0;
    mViewport.maxDepth = 1;

    mScissor.extent.width = 800;
    mScissor.extent.height = 600;

    VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.pViewports = &mViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &mScissor;

    /////////////////////////////////////////////////////////////////////////////////////////
    // dynamic state
    VkPipelineDynamicStateCreateInfo dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = 2;
    VkDynamicState states[7] = {
        VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT,
    };
    dynamicState.pDynamicStates = states;

    /////////////////////////////////////////////////////////////////////////////////////////
    // blend state
    VkPipelineColorBlendAttachmentState blendAttachments[8];
    int attachmentCount = 1;
    for (int i = 0; i < attachmentCount; i++)
    {
        VkPipelineColorBlendAttachmentState& attachment = blendAttachments[i];
        attachment.blendEnable = VK_TRUE;
        attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_MAX;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
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

    /////////////////////////////////////////////////////////////////////////////////////////
    // depth state
    VkPipelineDepthStencilStateCreateInfo depthstencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthstencil.depthTestEnable = VK_TRUE;
    depthstencil.depthWriteEnable = VK_TRUE;
    depthstencil.depthCompareOp = VK_COMPARE_OP_GREATER;
    depthstencil.stencilTestEnable = VK_TRUE;

    depthstencil.front.compareMask = 0;
    depthstencil.front.writeMask = 0xFF;
    depthstencil.front.reference = 0; // runtime supplied
    depthstencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthstencil.front.passOp = VK_STENCIL_OP_REPLACE;
    depthstencil.front.failOp = VK_STENCIL_OP_KEEP;
    depthstencil.front.depthFailOp = VK_STENCIL_OP_KEEP;

    depthstencil.back.compareMask = 0;
    depthstencil.back.writeMask = 0xFF;
    depthstencil.back.reference = 0; // runtime supplied
    depthstencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthstencil.back.passOp = VK_STENCIL_OP_REPLACE;
    depthstencil.back.failOp = VK_STENCIL_OP_KEEP;
    depthstencil.back.depthFailOp = VK_STENCIL_OP_KEEP;

    depthstencil.depthBoundsTestEnable = VK_FALSE;


    /////////////////////////////////////////////////////////////////////////////////////////
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
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    /////////////////////////////////////////////////////////////////////////////////////////
    // raster
    VkPipelineRasterizationStateCreateInfo rasterizer = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_TRUE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    /////////////////////////////////////////////////////////////////////////////////////////
    // MSAA
    VkPipelineMultisampleStateCreateInfo multisampling = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;


    /////////////////////////////////////////////////////////////////////////////////////////
    // stages
    VkPipelineShaderStageCreateInfo stages[static_cast<unsigned>(ShaderStage::Count)];
    int stageCount = 0;

    for (int i = 0; i < static_cast<unsigned>(ShaderStage::Count); i++)
    {
        ShaderStage shaderStage = static_cast<ShaderStage>(i);
        if (pipelineStateDesc.mShaderProgram.GetShader(shaderStage))
        {
            VkPipelineShaderStageCreateInfo& stageCreateInfo = stages[stageCount++];
            stageCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
            stageCreateInfo.pName = "main";
            stageCreateInfo.module = pipelineStateDesc.mShaderProgram.GetShader(shaderStage)->mShaderModule;

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

    VkPipeline retPipeline = VK_NULL_HANDLE;
    VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.layout = pipelineStateDesc.mShaderProgram.GetPipelineLayout()->mPipelineLayout;
    pipelineInfo.renderPass = nullptr;
    pipelineInfo.subpass = 0;

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

    VkResult res = vkCreateGraphicsPipelines(mDevice.mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &retPipeline);
    if (res != VK_SUCCESS)
    {
        std::cout << "Failed to create graphics pipeline!" << std::endl;
        return VK_NULL_HANDLE;
    }
    return retPipeline;
}