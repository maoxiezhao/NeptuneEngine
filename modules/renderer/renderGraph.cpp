#include "renderer.h"
#include "renderGraph.h"
#include "core\memory\memory.h"

#include <stack>

namespace VulkanTest
{
    struct Barrier
    {
        U32 resIndex = 0;
        VkPipelineStageFlags stages = 0;
        VkAccessFlags access = 0;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct PassBarrier
    {
        std::vector<Barrier> invalidate;
        std::vector<Barrier> flush;
    };

    struct ColorClearRequest
    {
        RenderPass* pass;
        VkClearColorValue* target;
        U32 index;
    };

    struct DepthClearRequest
    {
        RenderPass* pass;
        VkClearDepthStencilValue* target;
    };

    struct PhysicalPass
    {
        std::vector<U32> passes;
        std::vector<U32> physicalColorAttachments;

        std::vector<GPU::RenderPassInfo::SubPass> gpuSubPasses;
        GPU::RenderPassInfo gpuRenderPassInfo = {};
        std::vector<ColorClearRequest> colorClearRequests;
        DepthClearRequest depthClearRequest;
        U32 physicalDepthStencilAttachment = RenderResource::Unused;
    };

    ///////////////////////////////////////////////////////////////////////////////////
    
    struct RenderGraphImpl
    {
        GPU::DeviceVulkan* device;
        String backbufferSource;

        std::unordered_map<const char*, U32> nameToRenderPassIndex;
        std::vector<UniquePtr<RenderPass>> renderPasses;
        std::unordered_map<const char*, U32> nameToResourceIndex;
        std::vector<UniquePtr<RenderResource>> resources;

        std::vector<U32> passStack;
        std::vector<std::unordered_set<U32>> passDependency;
        std::vector<PassBarrier> passBarriers;

        void HandlePassRecursive(RenderPass& self, const std::unordered_set<U32>& writtenPass, U32 stackCount);
        void TraverseDependencies(RenderPass& renderPass, U32 stackCount);
        void ReorderRenderPasses(std::vector<U32>& passes);
        bool CheckPassDepend(U32 srcPass, U32 dstPass);
        void BuildPhysicalResources();
        void BuildPhysicalPasses();
        void BuildTransientResources();
        void BuildRenderPassInfo();
        void BuildBarriers();
        void BuildPhysicalBarriers();

        std::vector<ResourceDimensions> physicalDimensions;
        ResourceDimensions swapchainDimensions;
        U32 swapchainPhysicalIndex = 0;
        std::vector<PhysicalPass> physicalPasses;

        ResourceDimensions CreatePhysicalDimensions(const RenderTextureResource& res)
        {
            const AttachmentInfo& info = res.GetAttachmentInfo();
            ResourceDimensions newDim = {};
            newDim.layers = info.layers;
            newDim.samples = info.samples;
            newDim.format = info.format;
            newDim.queues = res.GetUsedQueues();
            newDim.imageUsage = res.GetImageUsage();
            newDim.name = res.GetName();

            switch (info.sizeType)
            {
            case AttachmentSizeType::Absolute:
                newDim.width = info.sizeX;
                newDim.height = info.sizeY;
                newDim.depth = info.sizeZ;
                break;

            case AttachmentSizeType::SwapchainRelative:
                newDim.width = U32(std::ceilf(info.sizeX * swapchainDimensions.width));
                newDim.height = U32(std::ceilf(info.sizeY * swapchainDimensions.height));
                newDim.depth = U32(std::ceilf(info.sizeZ * swapchainDimensions.depth));
                break;
            }

            if (newDim.format == VK_FORMAT_UNDEFINED)
                newDim.format = swapchainDimensions.format;

            newDim.levels = info.levels;
            return newDim;
        }
    };

    void RenderGraphImpl::HandlePassRecursive(RenderPass& self, const std::unordered_set<U32>& writtenPass, U32 stackCount)
    {
        if (writtenPass.empty())
            return;

        if (stackCount > renderPasses.size())
            return;

        stackCount++;

        auto selfIndex = self.GetIndex();
        for (U32 passIndex : writtenPass)
        {
            if (passIndex != selfIndex)
                passDependency[selfIndex].insert(passIndex);
        }

        for (U32 passIndex : writtenPass)
        {
            if (passIndex != selfIndex)
            {
                passStack.push_back(passIndex);
                TraverseDependencies(*renderPasses[passIndex], stackCount);
            }
        }
    }

    void RenderGraphImpl::TraverseDependencies(RenderPass& renderPass, U32 stackCount)
    {
        const RenderTextureResource* depthStencil = renderPass.GetInputDepthStencil();
        if (depthStencil != nullptr)
        {
            HandlePassRecursive(renderPass, depthStencil->GetWrittenPasses(), stackCount);
        }

        for (auto res : renderPass.GetInputTextures())
        {
            if (res.texture != nullptr)
                HandlePassRecursive(renderPass, res.texture->GetWrittenPasses(), stackCount);
        }
    }

    void RenderGraphImpl::ReorderRenderPasses(std::vector<U32>& passes)
    {
        // Filter passes
        std::reverse(passes.begin(), passes.end());
        {
            std::unordered_set<U32> seen;
            auto endIt = passes.begin();
            for (auto it = passes.begin(); it != passes.end(); it++)
            {
                if (!seen.count(*it))
                {
                    *endIt = *it;
                    endIt++;
                    seen.insert(*it);
                }
            }
            passes.erase(endIt, passes.end());
        }

        std::vector<U32> unscheduledPasses;
        unscheduledPasses.reserve(passes.size());
        std::swap(passes, unscheduledPasses);

        auto SchedulePass = [&](U32 index)
        {
            passes.push_back(unscheduledPasses[index]);
            std::move(unscheduledPasses.begin() + index + 1, unscheduledPasses.end(), unscheduledPasses.begin() + index);
            unscheduledPasses.pop_back();
        };
        SchedulePass(0);

        while (!unscheduledPasses.empty())
        {
            // Find best candidate
            U32 bestOverlapFactor = 0;
            U32 bestCandidate = 0;
            for (int i = 0; i < unscheduledPasses.size(); i++)
            {
                // Find pass which has shortest length of dependent path
                U32 overlapFactor = 0;
                for (auto rIt = unscheduledPasses.rbegin(); rIt != unscheduledPasses.rend(); rIt++)
                {
                    if (CheckPassDepend(*rIt, unscheduledPasses[i]))
                        break;
                    overlapFactor++;
                }
                if (overlapFactor <= bestOverlapFactor)
                    continue;
            
                // Check pass i without previous dependency
                bool isValid = true;
                for (int j = 0; j < i; j++)
                {
                    if (CheckPassDepend(i, j))
                    {
                        isValid = false;
                        break;
                    }
                }

                if (isValid)
                {
                    bestCandidate = i;
                    bestOverlapFactor = overlapFactor;
                }
            }
            SchedulePass(bestCandidate);
        }
    }

    bool RenderGraphImpl::CheckPassDepend(U32 srcPass, U32 dstPass)
    {
        if (srcPass == dstPass)
            return true;

        auto& dependencies = passDependency[dstPass];
        for (auto pass : dependencies)
        {
            if (CheckPassDepend(srcPass, pass))
                return true;
        }
        return false;
    }

    void RenderGraphImpl::BuildPhysicalResources()
    {
        U32 physicalIndex = 0;
        auto AddImagePhysicalDimension = [&](RenderTextureResource* res) 
        {
            if (res->GetPhysicalIndex() == RenderResource::Unused)
            {
                physicalDimensions.push_back(CreatePhysicalDimensions(*res));
                res->SetPhysicalIndex(physicalIndex++);
            }
            else
            {
                physicalDimensions[res->GetPhysicalIndex()].queues |= res->GetUsedQueues();
                physicalDimensions[res->GetPhysicalIndex()].imageUsage |= res->GetImageUsage();
            }
        };

        for (auto& passIndex : passStack)
        {
            auto& pass = renderPasses[passIndex];

            for (auto& input : pass->GetInputTextures())
            {
                AddImagePhysicalDimension(input.texture);
            }

            for (auto& output : pass->GetOutputColors())
            {
                AddImagePhysicalDimension(output);
            }

            auto* dsInput = pass->GetInputDepthStencil();
            auto* dsOutput = pass->GetOutputDepthStencil();
            if (dsInput != nullptr)
            {
                AddImagePhysicalDimension(dsInput);
            
                if (dsOutput != nullptr)
                {
                    if (dsOutput->GetPhysicalIndex() == RenderResource::Unused)
                        dsOutput->SetPhysicalIndex(dsInput->GetPhysicalIndex());

                    physicalDimensions[dsOutput->GetPhysicalIndex()].queues |= dsOutput->GetUsedQueues();
                    physicalDimensions[dsOutput->GetPhysicalIndex()].imageUsage |= dsOutput->GetImageUsage();
                }
            }
            if (dsOutput != nullptr)
            {
                AddImagePhysicalDimension(dsOutput);
            }
        }
    }

    void RenderGraphImpl::BuildPhysicalPasses()
    {
        physicalPasses.clear();

        // Try to merge adjacent render passes together
        // 1. Both graphics queue
        // 2. Have some color / depth / input attachments
        // 3. No input dependency

        auto FindTexture = [&](const std::vector<RenderTextureResource*> resources, const RenderTextureResource* target) {
            if (target == nullptr)
                return false;

            return std::find(resources.begin(), resources.end(), target) != resources.end();
        };
        auto FindBuffer = [&](const std::vector<RenderBufferResource*> resources, const RenderBufferResource* target) {
            if (target == nullptr)
                return false;

            return std::find(resources.begin(), resources.end(), target) != resources.end();
        };
        auto CheckMergeEnable = [&](const RenderPass& prevPass, const RenderPass& nextPass)
        {
            if ((prevPass.GetQueue() & (U32)RenderGraphQueueFlag::Compute) || prevPass.GetQueue() != nextPass.GetQueue())
                return false;

            // Check dependency
            for (auto& input : nextPass.GetInputTextures())
            {
                if (FindTexture(prevPass.GetOutputColors(), input.texture))
                    return false;
                if (prevPass.GetOutputDepthStencil() == input.texture)
                    return false;
            }
            for (auto& input : nextPass.GetInputBuffers())
            {
                if (FindBuffer(prevPass.GetOutputBuffers(), input))
                    return false;
            }

            // Check have same depth stencil
            const auto DifferentAttachment = [](const RenderResource* a, const RenderResource* b) {
                return a && b && a->GetPhysicalIndex() != b->GetPhysicalIndex();
            };
            if (DifferentAttachment(nextPass.GetInputDepthStencil(), prevPass.GetInputDepthStencil()))
                return false;
            if (DifferentAttachment(nextPass.GetOutputDepthStencil(), prevPass.GetInputDepthStencil()))
                return false;
            if (DifferentAttachment(nextPass.GetInputDepthStencil(), prevPass.GetOutputDepthStencil()))
                return false;
            if (DifferentAttachment(nextPass.GetOutputDepthStencil(), prevPass.GetOutputDepthStencil()))
                return false;

            const auto SameAttachment = [](const RenderResource* a, const RenderResource* b) {
                return a && b && a->GetPhysicalIndex() == b->GetPhysicalIndex();
            };
            // Keep depth on tile.
            if (SameAttachment(nextPass.GetInputDepthStencil(), prevPass.GetInputDepthStencil()) ||
                SameAttachment(nextPass.GetInputDepthStencil(), prevPass.GetOutputDepthStencil()))
            {
                return true;
            }

            return false;
        };

        PhysicalPass physicalPass = {};
        for (U32 i = 0; i < passStack.size(); i++)
        {
            U32 mergeEnd = i + 1;
            for (; mergeEnd < passStack.size(); mergeEnd++)
            {
                bool canMerge = true;
                for (U32 mergeStart = i; mergeStart < mergeEnd; mergeStart++)
                {
                    RenderPass& prevPass = *renderPasses[passStack[mergeStart]];
                    RenderPass& nextPass = *renderPasses[passStack[mergeEnd]];
                    if (!CheckMergeEnable(prevPass, nextPass))
                    {
                        canMerge = false;
                        break;
                    }
                }

                if (!canMerge)
                    break;
            }

            physicalPass.passes.insert(physicalPass.passes.end(), passStack.begin() + i, passStack.begin() + mergeEnd);
            physicalPasses.push_back(std::move(physicalPass));
            i = mergeEnd;
        }

        for (U32 index = 0; index < physicalPasses.size(); index++)
        {
            for (auto& passIndex : physicalPasses[index].passes)
                renderPasses[passIndex]->SetPhysicalIndex(index);
        }
    }

    void RenderGraphImpl::BuildTransientResources()
    {
        std::vector<U32> physicalResUsed(physicalDimensions.size());
        for (auto& used : physicalResUsed)
            used = RenderPass::Unused;

        for (auto& res : physicalDimensions)
        {
            // Buffer are never transient
            if ((res.imageUsage & VK_IMAGE_USAGE_STORAGE_BIT) || res.bufferInfo.size != 0)
                res.isTransient = false;
            else
                res.isTransient = true;
        }

        for (auto& res : resources)
        {
            // Buffer are never transient
            if (res->GetResourceType() != ResourceType::Texture)
                continue;

            U32 resPhysicalIndex = res->GetPhysicalIndex();
            if (resPhysicalIndex == RenderResource::Unused)
                continue;

            for (auto& passIndex : res->GetWrittenPasses())
            {
                auto& pass = *renderPasses[passIndex];
                if (pass.GetPhysicalIndex() != RenderPass::Unused)
                {
                    if (physicalResUsed[resPhysicalIndex] != RenderPass::Unused && physicalResUsed[resPhysicalIndex] != pass.GetPhysicalIndex())
                    {
                        physicalDimensions[resPhysicalIndex].isTransient = false;
                        break;
                    }
                    physicalResUsed[resPhysicalIndex] = pass.GetPhysicalIndex();
                }
            }

            for (auto& passIndex : res->GetReadPasses())
            {
                auto& pass = *renderPasses[passIndex];
                if (pass.GetPhysicalIndex() != RenderPass::Unused)
                {
                    if (physicalResUsed[resPhysicalIndex] != RenderPass::Unused && physicalResUsed[resPhysicalIndex] != pass.GetPhysicalIndex())
                    {
                        physicalDimensions[resPhysicalIndex].isTransient = false;
                        break;
                    }
                    physicalResUsed[resPhysicalIndex] = pass.GetPhysicalIndex();
                }
            }
        }
    }

    void RenderGraphImpl::BuildRenderPassInfo()
    {
        for (auto& physicalPass : physicalPasses)
        {
            physicalPass.gpuSubPasses.resize(physicalPass.passes.size());

            // Create render pass info
            auto& renderPassInfo = physicalPass.gpuRenderPassInfo;
            renderPassInfo.numSubPasses = physicalPass.gpuSubPasses.size();
            renderPassInfo.subPasses = physicalPass.gpuSubPasses.data();
            renderPassInfo.clearAttachments = 0;
            renderPassInfo.loadAttachments = 0;
            renderPassInfo.storeAttachments = 0;

            auto& colorAttachements = physicalPass.physicalColorAttachments;
            colorAttachements.clear();
            auto AddColorAttachment = [&](U32 index) {
                auto it = std::find(colorAttachements.begin(), colorAttachements.end(), index);
                if (it != colorAttachements.end())
                    return std::make_pair((U32)(it - colorAttachements.begin()), false);

                colorAttachements.push_back(index);
                return std::make_pair((U32)(colorAttachements.size() - 1), true);
            };
            for (auto& passIndex : physicalPass.passes)
            {
                auto& pass = *renderPasses[passIndex];
                auto& subPass = physicalPass.gpuSubPasses[&passIndex - physicalPass.passes.data()];

                // Add color attachments
                auto& outputColors = pass.GetOutputColors();
                subPass.numColorAattachments = outputColors.size();
                for (int i = 0; i < subPass.numColorAattachments; i++)
                {
                    auto ret = AddColorAttachment(outputColors[i]->GetPhysicalIndex());
                    subPass.colorAttachments[i] = ret.first;
                    if (ret.second)
                    {
                        // This is the first tiem to use color attachment
                        bool hasColorInput = false;
                        if (hasColorInput)
                        {
                            renderPassInfo.loadAttachments |= 1u << ret.first;
                        }
                        else
                        {
                            if (pass.GetClearColor(i))
                            {
                                renderPassInfo.clearAttachments |= 1u << ret.first;
                                physicalPass.colorClearRequests.push_back({
                                    &pass,
                                    &renderPassInfo.clearColor[ret.first],
                                    (U32)i});
                            }
                        }

                    }  
                }

                // Add depth stencil
                auto AddUniqueDS = [&](unsigned index) {
                    bool isNewAttachment = physicalPass.physicalDepthStencilAttachment == RenderResource::Unused;
                    physicalPass.physicalDepthStencilAttachment = index;
                    return std::make_pair(index, isNewAttachment);
                };
                auto* dsInput = pass.GetInputDepthStencil();
                auto* dsOutput = pass.GetOutputDepthStencil();
                if (dsInput && dsOutput)
                {      
                    auto ret = AddUniqueDS(dsOutput->GetPhysicalIndex());
                    if (ret.second)
                        renderPassInfo.loadAttachments |= 1u << ret.first;

                    renderPassInfo.opFlags |= GPU::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
                    subPass.depthStencilMode = GPU::DepthStencilMode::ReadWrite;
                }
                else if (dsOutput)
                {
                    auto ret = AddUniqueDS(dsOutput->GetPhysicalIndex());
                    if (ret.second && pass.GetClearDepthStencil())
                    {
                        renderPassInfo.opFlags |= GPU::RENDER_PASS_OP_CLEAR_DEPTH_STENCIL_BIT;
                        physicalPass.depthClearRequest.pass = &pass;
                        physicalPass.depthClearRequest.target = &renderPassInfo.clearDepthStencil;
                    }

                    renderPassInfo.opFlags |= GPU::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
                    subPass.depthStencilMode = GPU::DepthStencilMode::ReadWrite;
                }
                else if (dsInput)
                {
                    auto ret = AddUniqueDS(dsInput->GetPhysicalIndex());
                    if (ret.second)
                    {
                        renderPassInfo.opFlags |= 
                            GPU::RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT |
                            GPU::RENDER_PASS_OP_LOAD_DEPTH_STENCIL_BIT;

                        // Check depth is preserve
                        U32 currentPhysicalPass = U32(&physicalPass - physicalPasses.data());
                        const auto CheckPreserve = [this, currentPhysicalPass](const RenderResource& ds) -> bool {
                            for (auto& readPass : ds.GetReadPasses())
                                if (renderPasses[readPass]->GetPhysicalIndex() > currentPhysicalPass)
                                    return true;
                            return false;
                        };
                        if (CheckPreserve(*dsInput))
                            renderPassInfo.opFlags |= GPU::RENDER_PASS_OP_STORE_DEPTH_STENCIL_BIT;
                    }

                    subPass.depthStencilMode = GPU::DepthStencilMode::ReadOnly;
                }
                else
                {
                    subPass.depthStencilMode = GPU::DepthStencilMode::None;
                }
            }

            // Add input attachments
            auto AddInputAttachment = [&](U32 index) {
                if (index == physicalPass.physicalDepthStencilAttachment)
                    return std::make_pair(U32(colorAttachements.size()), false); // N + 1 attachment used for depth.
                else
                    return AddColorAttachment(index);
            };
            for (auto& passIndex : physicalPass.passes)
            {
                auto& pass = *renderPasses[passIndex];
                auto& subPass = physicalPass.gpuSubPasses[&passIndex - physicalPass.passes.data()];

                auto& inputAttachments = pass.GetInputAttachments();
                subPass.numInputAttachments = inputAttachments.size();
                for (int i = 0; i < subPass.numColorAattachments; i++)
                {     
                    auto ret = AddInputAttachment(inputAttachments[i]->GetPhysicalIndex());
                    subPass.inputAttachments[i] = ret.first;
                    if (ret.second)
                        renderPassInfo.loadAttachments |= 1u << ret.first;
                }
            }
            
            renderPassInfo.numColorAttachments = physicalPass.physicalColorAttachments.size();
        }
    }

    void RenderGraphImpl::BuildBarriers()
    {
        passBarriers.clear();
        passBarriers.reserve(passStack.size());

        auto GetAccessBarrier = [&](std::vector<Barrier> barriers, U32 index)->Barrier& {
            auto it = std::find_if(barriers.begin(), barriers.end(), [&](const Barrier& barrier) {
                return barrier.resIndex == index;
            });
            if (it != barriers.end())
                return *it;

            barriers.push_back({index, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED });
            return barriers.back();
        };
        for (auto& passIndex : passStack)
        {
            auto& pass = *renderPasses[passIndex];
            PassBarrier passBarrier;

            /////////////////////////////////////////////////////////////////////
            // Input
            // Push into validate list
            auto GetInvalidateAccess = [&](U32 index)->Barrier& {
                return GetAccessBarrier(passBarrier.invalidate, index);
            };
            for (auto& tex : pass.GetInputTextures())
            {
                Barrier& barrier = GetInvalidateAccess(tex.texture->GetPhysicalIndex());
                barrier.access |= tex.access;
                barrier.stages |= tex.stages;
                barrier.layout = tex.layout;
            }

            /////////////////////////////////////////////////////////////////////
            // Output
            // Push into flush list
            auto GetFlushAccess = [&](U32 index)->Barrier& {
                return GetAccessBarrier(passBarrier.flush, index);
            };
            for (auto& color : pass.GetOutputColors())
            {
                Barrier& barrier = GetFlushAccess(color->GetPhysicalIndex());
                barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                // Set VK_IMAGE_LAYOUT_GENERAL if the attachment is also bound as an input attachment(ReadTexture)
                if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
                    barrier.layout == VK_IMAGE_LAYOUT_GENERAL)
                {
                    barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                }
                else if (barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED)
                {
                    barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }

            /////////////////////////////////////////////////////////////////////
            // Depth stencil
            auto* dsInput = pass.GetInputDepthStencil();
            auto* dsOutput = pass.GetOutputDepthStencil();
            if (dsInput && dsOutput)
            {
                Barrier& inputBarrier = GetInvalidateAccess(dsInput->GetPhysicalIndex());
                if (inputBarrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    inputBarrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                else
                    inputBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                inputBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                inputBarrier.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

                Barrier& outputBarrier = GetFlushAccess(dsOutput->GetPhysicalIndex());
                outputBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                outputBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                outputBarrier.stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
            else if (dsOutput)
            {
                Barrier& barrier = GetFlushAccess(dsOutput->GetPhysicalIndex());
                if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                else
                    barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                
                barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                barrier.stages |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }
            else if (dsInput)
            {
                Barrier& barrier = GetInvalidateAccess(dsInput->GetPhysicalIndex());
                barrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                barrier.stages |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            }

            passBarriers.push_back(std::move(passBarrier));
        }
    }

    void RenderGraphImpl::BuildPhysicalBarriers()
    {
        struct ResourceState
        {

        };
        std::vector<ResourceState> resourceStates;
        resourceStates.resize(physicalDimensions.size());

        for (U32 physicalPassIndex = 0; physicalPassIndex < physicalPasses.size(); physicalPassIndex++)
        {
            auto& physicalPass = physicalPasses[physicalPassIndex];
            for (auto& subpass : physicalPass.passes)
            {
                auto& pass = renderPasses[subpass];
                auto& passBarrier = passBarriers[subpass];
                auto& invalidate = passBarrier.invalidate;
                auto& flush = passBarrier.flush;

                // Invalidate
                for (auto& barrier : invalidate)
                {

                }

                // Flush
                for (auto& barrier : flush)
                {

                }
            }
        }

        for (auto& resState : resourceStates)
        {

        }
    }

    ///////////////////////////////////////////////////////////////////////////////////
    
    RenderPass::RenderPass(RenderGraph& graph_, U32 index_, U32 queue_) :
        graph(graph_),
        index(index_),
        queue(queue_)
    {
    }
    
    RenderPass::~RenderPass()
    {
    }

    RenderTextureResource& RenderPass::ReadTexture(const char* name, VkPipelineStageFlags stages)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassRead(index);
        res.SetImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT);

        auto it = std::find_if(inputTextures.begin(), inputTextures.end(), 
            [&](const AccessedTextureResource& a) {
                return a.texture == &res;
            });
        if (it != inputTextures.end())
            return res;

        AccessedTextureResource acc = {};
        acc.texture = &res;
        acc.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        acc.access = VK_ACCESS_SHADER_READ_BIT;

        if (stages != 0)
            acc.stages = stages;
        else if ((queue & (U32)RenderGraphQueueFlag::Compute) != 0)
            acc.stages = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        else
            acc.stages = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        inputTextures.push_back(acc);
        return res;
    }

    RenderTextureResource& RenderPass::ReadDepthStencil(const char* name)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassRead(index);
        res.SetImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        inputDepthStencil = &res;
        return res;
    }

    RenderTextureResource& RenderPass::WriteColor(const char* name, const AttachmentInfo& info)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        res.SetImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        res.SetAttachmentInfo(info);
        outputColors.push_back(&res);
        return res;
    }

    RenderTextureResource& RenderPass::WriteDepthStencil(const char* name, const AttachmentInfo& info)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        res.SetImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        res.SetAttachmentInfo(info);
        outputDepthStencil = &res;
        return res;
    }

    RenderBufferResource& RenderPass::ReadStorageBuffer(const char* name)
    {
        auto& res = graph.GetOrCreateBuffer(name);
        res.AddUsedQueue(queue);
        res.PassRead(index);
        inputBuffers.push_back(&res);
        return res;
    }

    RenderBufferResource& RenderPass::WriteStorageBuffer(const char* name, const BufferInfo& info, const char* input)
    {
        auto& res = graph.GetOrCreateBuffer(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        outputBuffers.push_back(&res);
        return res;
    }

    RenderGraph::RenderGraph()
    {
        impl = CJING_NEW(RenderGraphImpl);
    }

    RenderGraph::~RenderGraph()
    {
        CJING_SAFE_DELETE(impl);
    }

    void RenderGraph::Reset()
    {
        impl->nameToRenderPassIndex.clear();
        impl->renderPasses.clear();
    }

    void RenderGraph::SetDevice(GPU::DeviceVulkan* device)
    {
        impl->device = device;
    }

    RenderPass& RenderGraph::AddRenderPass(const char* name, RenderGraphQueueFlag queueFlag)
    {
        auto it = impl->nameToRenderPassIndex.find(name);
        if (it != impl->nameToRenderPassIndex.end())
            return *impl->renderPasses[it->second];

        U32 index = impl->renderPasses.size();
        impl->renderPasses.emplace_back(CJING_NEW(RenderPass)(*this, index, (U32)queueFlag));
        auto& pass = *impl->renderPasses.back();
        pass.name = name;
        impl->nameToRenderPassIndex[name] = index;
        return pass;
    }

    void RenderGraph::SetBackBufferSource(const char* name)
    {
        impl->backbufferSource = name;
    }

    void RenderGraph::Bake()
    {
        // Find back buffer source
        if (impl->backbufferSource.empty())
            return;

        auto it = impl->nameToResourceIndex.find(impl->backbufferSource);
        if (it == impl->nameToResourceIndex.end())
        {
            Logger::Error("Backbuffer source does not exist.");
            return;
        }

        std::vector<UniquePtr<RenderPass>>& renderPasses = impl->renderPasses;
        RenderResource& backbuffer = *impl->resources[it->second];

        // Traverse graph dependices
        impl->passStack.clear();
        for (auto& passIndex : backbuffer.GetWrittenPasses())
            impl->passStack.push_back(passIndex);

        auto tempStack = impl->passStack;
        for (auto& passIndex : tempStack)
        {
            auto& pass = *renderPasses[passIndex];
            impl->TraverseDependencies(pass, 0);
        }

        // Reorder render passes
        impl->ReorderRenderPasses(impl->passStack);

        // Now, we have a linear list of passes which submit in-order and obey the dependencies

        // Build the physical resources we really need
        impl->BuildPhysicalResources();

        // Merge adjacent passes
        impl->BuildPhysicalPasses();

        // Build transient resources which is only used in a single physical pass
        impl->BuildTransientResources();

        // Build GPU::RenderPassInfos
        impl->BuildRenderPassInfo();

        // Build logical barriers per pass
        impl->BuildBarriers();

        // Set swapchain physcical index
        impl->swapchainPhysicalIndex = backbuffer.GetPhysicalIndex();
        auto& backbufferDim = impl->physicalDimensions[impl->swapchainPhysicalIndex];
        bool canAliasBackbuffer = (backbufferDim.queues & (U32)RenderGraphQueueFlag::Compute == 0) && backbufferDim.isTransient;
        if (!canAliasBackbuffer || backbufferDim != impl->swapchainDimensions)
        {
            impl->swapchainPhysicalIndex = RenderResource::Unused;
            backbufferDim.queues |= (U32)RenderGraphQueueFlag::Graphics;
            backbufferDim.imageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            backbufferDim.isTransient = false;
        }
        else
        {
            backbufferDim.isTransient = true;
        }

        // Build physical barriers which we really need
        impl->BuildPhysicalBarriers();
    }
    
    void RenderGraph::Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle)
    {

    }
    
    void RenderGraph::Log()
    {
        
    }

    RenderTextureResource& RenderGraph::GetOrCreateTexture(const char* name)
    {
        auto it = impl->nameToResourceIndex.find(name);
        if (it != impl->nameToResourceIndex.end())
            return *static_cast<RenderTextureResource*>(impl->resources[it->second].Get());

        U32 index = impl->resources.size();
        impl->resources.emplace_back(CJING_NEW(RenderTextureResource)(index));
        auto& res = *impl->resources.back();
        res.name = name;
        impl->nameToResourceIndex[name] = index;
        return *static_cast<RenderTextureResource*>(&res);
    }

    RenderBufferResource& RenderGraph::GetOrCreateBuffer(const char* name)
    {
        auto it = impl->nameToResourceIndex.find(name);
        if (it != impl->nameToResourceIndex.end())
            return *static_cast<RenderBufferResource*>(impl->resources[it->second].Get());

        U32 index = impl->resources.size();
        impl->resources.emplace_back(CJING_NEW(RenderBufferResource)(index));
        auto& res = *impl->resources.back();
        res.name = name;
        impl->nameToResourceIndex[name] = index;
        return *static_cast<RenderBufferResource*>(&res);
    }

    void RenderGraph::SetBackbufferDimension(const ResourceDimensions& dim)
    {
        impl->swapchainDimensions = dim;
    }

}
