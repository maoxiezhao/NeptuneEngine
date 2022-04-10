#include "renderer.h"
#include "renderGraph.h"
#include "gpu\vulkan\typeToString.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"

#include <stack>

namespace VulkanTest
{
#define RENDER_GRAPH_LOGGING_LEVEL (0)

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
        std::vector<U32> physicalColorAttachments;  // PhysicalResIndex

        std::vector<GPU::RenderPassInfo::SubPass> gpuSubPasses;
        GPU::RenderPassInfo gpuRenderPassInfo = {};
        std::vector<ColorClearRequest> colorClearRequests;
        DepthClearRequest depthClearRequest;
        U32 physicalDepthStencilAttachment = (U32)RenderResource::Unused;
        std::vector<Barrier> invalidate;
        std::vector<Barrier> flush;
        std::vector<unsigned> discards;
        std::vector<std::pair<U32, U32>> aliasTransfers;
    };

    struct RenderGraphEvent
    {
        GPU::EventPtr ent;
        GPU::SemaphorePtr waitGraphicsSemaphore;
        GPU::SemaphorePtr waitComputeSemaphore;
        VkAccessFlags invalidatedInStages[32] = {};
        U32 flushAccess = 0;
        VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    struct GPUPassSubmissionState
    {
        const char* name = nullptr;
        bool active = false;
        GPU::CommandListPtr cmd;
        bool isGraphics = true;
        GPU::QueueType queueType = GPU::QueueType::QUEUE_TYPE_GRAPHICS;
        std::vector<VkBufferMemoryBarrier> eventBufferBarriers;
        std::vector<VkImageMemoryBarrier> eventImageBarriers;
        std::vector<VkImageMemoryBarrier> immediateImageBarriers;
        std::vector<VkImageMemoryBarrier> handoverBarriers;
        std::vector<VkEvent> events;
        std::vector<GPU::SemaphorePtr> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitSemaphoreStages;

        VkPipelineStageFlags srcStages = 0;
        VkPipelineStageFlags dstStages = 0;
        VkPipelineStageFlags immediateDstStages = 0;
        VkPipelineStageFlags handoverStages = 0;

        bool needSubmissionSemaphore = false;
        VkPipelineStageFlags signalEventStages = 0;
        GPU::EventPtr signalEvent;
        GPU::SemaphorePtr graphicsSemaphore;
        GPU::SemaphorePtr computeSemaphore;

        Jobsystem::JobHandle renderingDependency;
        Jobsystem::JobHandle submissionHandle;

        void EmitPrePassBarriers();
        void EmitPostPassBarriers();
        void Submit();
    };

    ///////////////////////////////////////////////////////////////////////////////////
    
    struct RenderGraphImpl
    {
        GPU::DeviceVulkan* device;
        String backbufferSource;

        std::unordered_map<String, U32> nameToRenderPassIndex;
        std::vector<UniquePtr<RenderPass>> renderPasses;
        std::unordered_map<String, U32> nameToResourceIndex;
        std::vector<UniquePtr<RenderResource>> resources;

        std::vector<U32> passStack;
        std::vector<std::unordered_set<U32>> passDependency;
        std::vector<PassBarrier> passBarriers;

        // Bake methods
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
        void BuildAliases();

        // Runtime methods
        void SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain);
        void HandleInvalidateBarrier(const Barrier& barrier, GPUPassSubmissionState& state, bool isGraphicsQueue);
        void HandleFlushBarrier(const Barrier& barrier, GPUPassSubmissionState& state);  
        void DoGraphicsCommands(GPU::CommandList& cmd, PhysicalPass& physicalPass, GPUPassSubmissionState* state);
        void DoComputeCommands(GPU::CommandList& cmd, PhysicalPass& physicalPass, GPUPassSubmissionState* state);
        void Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle);

        // Debug
        void Log();
        String DebugExportGraphviz();

        std::vector<ResourceDimensions> physicalDimensions;
        std::vector<U32> physicalAliases;
        ResourceDimensions swapchainDimensions;
        U32 swapchainPhysicalIndex = 0;
        std::vector<PhysicalPass> physicalPasses;
        std::vector<RenderGraphEvent> physicalEvents;
        std::vector<GPU::ImageView*> physicalAttachments;
        std::vector<GPU::ImagePtr> physicalImages;
        std::vector<GPU::BufferPtr> physicalBuffers;
        std::vector<GPUPassSubmissionState> submissionStates;

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
                newDim.width = (U32)info.sizeX;
                newDim.height = (U32)info.sizeY;
                newDim.depth = (U32)info.sizeZ;
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
        for (U32 i = 0; i < passStack.size();)
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
            if (res.IsBufferLikeRes())
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
            physicalPass.colorClearRequests.clear();
            physicalPass.depthClearRequest = {};

            // Create render pass info
            auto& renderPassInfo = physicalPass.gpuRenderPassInfo;
            renderPassInfo.numSubPasses = physicalPass.gpuSubPasses.size();
            renderPassInfo.subPasses = physicalPass.gpuSubPasses.data();
            renderPassInfo.clearAttachments = 0;
            renderPassInfo.loadAttachments = 0;
            renderPassInfo.storeAttachments = ~0u;

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
                        // This is the first time to use color attachment
                        bool hasColorInput = !pass.GetInputColors().empty() && pass.GetInputColors()[i] != nullptr;
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
                for (int i = 0; i < subPass.numInputAttachments; i++)
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

        auto GetAccessBarrier = [&](std::vector<Barrier>& barriers, U32 index)->Barrier& {
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
                ASSERT(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED);
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
                if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || barrier.layout == VK_IMAGE_LAYOUT_GENERAL)
                {
                    barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                }
                else 
                {
                    ASSERT(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED);
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
            VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkAccessFlags invalidatedTypes = 0;
            VkAccessFlags flushedTypes = 0;
            VkPipelineStageFlags invalidatedStages = 0;
            VkPipelineStageFlags flushedStages = 0;
        };
        std::vector<ResourceState> resourceStates;
        resourceStates.reserve(physicalDimensions.size());

        for (U32 physicalPassIndex = 0; physicalPassIndex < physicalPasses.size(); physicalPassIndex++)
        {
            resourceStates.clear();
            resourceStates.resize(physicalDimensions.size());

            // In multipass, only the first and last barriers need to be considered externally.
            auto& physicalPass = physicalPasses[physicalPassIndex];
            for (auto& subpass : physicalPass.passes)
            {
                auto& passBarrier = passBarriers[subpass];
                auto& invalidates = passBarrier.invalidate;
                auto& flushes = passBarrier.flush;

                // Invalidate
                for (auto& invalidate : invalidates)
                {
                    auto& resState = resourceStates[invalidate.resIndex];
                    auto& dim = physicalDimensions[invalidate.resIndex];
                    if (dim.isTransient || invalidate.resIndex == swapchainPhysicalIndex)
                        continue;

                    // Find a physical pass first use of the resource
                    if (resState.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                    {
                        resState.invalidatedTypes |= invalidate.access;
                        resState.invalidatedStages |= invalidate.stages;

                        if (dim.IsStorageImage())
                            resState.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
                        else
                            resState.initialLayout = invalidate.layout;
                    }

                    if (dim.IsStorageImage())
                        resState.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
                    else
                        resState.finalLayout = invalidate.layout;

                    resState.flushedStages = 0;
                    resState.flushedTypes = 0;
                }

                // Flush
                for (auto& flush : flushes)
                {
                    auto& resState = resourceStates[flush.resIndex];
                    auto& dim = physicalDimensions[flush.resIndex];
                    if (dim.isTransient || flush.resIndex == swapchainPhysicalIndex)
                        continue;

                    // Find a physical pass last use use of the resource
                    resState.flushedTypes |= flush.access;
                    resState.flushedStages |= flush.stages;

                    if (dim.IsStorageImage())
                        resState.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
                    else
                        resState.finalLayout = flush.layout;

                    // No invalidation before first flush, invalidate first.
                    if (resState.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                    {
                        resState.initialLayout = flush.layout;
                        resState.invalidatedStages = flush.stages;

                        VkAccessFlags flags = flush.access;
                        if (flags & VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                            flags |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                        if (flags & VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                            flags |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                        if (flags & VK_ACCESS_SHADER_WRITE_BIT)
                            flags |= VK_ACCESS_SHADER_READ_BIT;
                        resState.invalidatedTypes = flags;

                        // Resource is not used in current pass, so we discard the resource
                        physicalPass.discards.push_back(flush.resIndex);
                    }
                }
            }

            for (auto& resState : resourceStates)
            {
                // Resource was not used in the pass
                if (resState.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
                    resState.finalLayout == VK_IMAGE_LAYOUT_UNDEFINED)
                    continue;

                U32 resIndex = U32(&resState - resourceStates.data());
                physicalPass.invalidate.push_back({
                    resIndex, 
                    resState.invalidatedStages, 
                    resState.invalidatedTypes, 
                    resState.initialLayout
                });

                if (resState.flushedTypes != 0)
                {
                    physicalPass.flush.push_back({
                        resIndex, 
                        resState.flushedStages, 
                        resState.flushedTypes, 
                        resState.finalLayout
                    });
                }
                else if (resState.invalidatedTypes != 0)
                {
                    // Only read in this pass, add a flush with 0 access bits to protect before it can be written
                    physicalPass.flush.push_back({
                        resIndex, 
                        resState.invalidatedStages, 
                        0, 
                        resState.finalLayout
                    });
                }
            }
        }
    }

    void RenderGraphImpl::BuildAliases()
    {
        struct ResourceRange
        {
            U32 firstReadPass = ~0;
            U32 lastReadPass = 0;
            U32 firstWritePass = ~0;
            U32 lastWritePass = 0;

            bool HasWriter() const
            {
                return firstWritePass <= lastWritePass;
            }

            bool HasReader() const
            {
                return firstReadPass <= lastReadPass;
            }

            bool IsUsed() const
            {
                return HasWriter() || HasReader();
            }

            bool CanAlias() const
            {
                // If we read before we have completely written to a resource we need to preserve it, so no alias is possible.
                return !(HasReader() && HasWriter() && firstReadPass <= firstWritePass);
            }

            I32 GetFirstUsed()
            {
                I32 firstUsedPass = INT_MAX;
                firstUsedPass = HasWriter() ? std::min(firstUsedPass, (I32)firstWritePass) : firstUsedPass;
                firstUsedPass = HasReader() ? std::min(firstUsedPass, (I32)firstReadPass) : firstUsedPass;
                return firstUsedPass;
            }

            I32 GetLastUsedPass()
            {
                I32 lastUsedPass = -1;
                lastUsedPass = HasWriter() ? std::max(lastUsedPass, (I32)lastWritePass) : lastUsedPass;
                lastUsedPass = HasReader() ? std::max(lastUsedPass, (I32)lastReadPass) : lastUsedPass;
                return lastUsedPass;
            }

            bool CheckDisjointRange(ResourceRange range)
            {
                if (!IsUsed() || !range.IsUsed())
                    return false;
                if (!CanAlias() || !range.CanAlias())
                    return false;

                return (GetLastUsedPass() < range.GetFirstUsed() ||
                        range.GetLastUsedPass() < GetFirstUsed());
            }
        };
        std::vector<ResourceRange> resourceRanges(physicalDimensions.size());

        auto AddReaderPass = [&](const RenderTextureResource* res, U32 passIndex) {
            if (res == nullptr || passIndex == RenderPass::Unused || res->GetPhysicalIndex() == RenderResource::Unused)
                return;

            auto& range = resourceRanges[res->GetPhysicalIndex()];
            range.firstReadPass = std::min(range.firstReadPass, passIndex);
            range.lastReadPass = std::max(range.lastReadPass, passIndex);
        
        };
        auto AddWriterPass = [&](const RenderTextureResource* res, U32 passIndex) {
            if (res == nullptr || passIndex == RenderPass::Unused || res->GetPhysicalIndex() == RenderResource::Unused)
                return;

            auto& range = resourceRanges[res->GetPhysicalIndex()];
            range.firstWritePass = std::min(range.firstWritePass, passIndex);
            range.lastWritePass = std::max(range.lastWritePass, passIndex);
        };

        // Add reader passes and writer passess
        for (auto& passIndex : passStack)
        {
            auto& pass = renderPasses[passIndex];
            for (auto& tex : pass->GetInputTextures())
                AddReaderPass(tex.texture, pass->GetPhysicalIndex());

            for (auto& output : pass->GetOutputColors())
                AddWriterPass(output, pass->GetPhysicalIndex());
        }

        physicalAliases.resize(physicalDimensions.size());
        for (auto& v : physicalAliases)
            v = RenderResource::Unused;

        std::vector<std::vector<U32>> aliasChain(physicalDimensions.size());
        for (int i = 0; i < physicalDimensions.size(); i++)
        {
            auto& physicalRes = physicalDimensions[i];
            if (physicalRes.IsBuffer())
                continue;

            auto& range = resourceRanges[i];
            
            // Only alias with previous resource
            for (int j = 0; j < i; j++)
            {
                if (physicalDimensions[i] == physicalDimensions[j])
                {
                    if (physicalDimensions[i].queues != physicalDimensions[j].queues)
                        continue;

                    if (range.CheckDisjointRange(resourceRanges[j]))
                    {
                        physicalAliases[i] = j;

                        if (aliasChain[j].empty())
                            aliasChain[j].push_back(j);
                        aliasChain[j].push_back(i);

                        U32 mergedImageUsage = physicalDimensions[i].imageUsage | physicalDimensions[j].imageUsage;
                        physicalDimensions[i].imageUsage |= mergedImageUsage;
                        physicalDimensions[j].imageUsage |= mergedImageUsage;
                    }
                }
            }
        }

        for (auto& chain : aliasChain)
        {
            if (chain.empty())
                continue;

            for (int i = 0; i < chain.size(); i++)
            {
                I32 lastUsedPass = resourceRanges[chain[i]].GetLastUsedPass();
                if (lastUsedPass < 0)
                    continue;

                auto& physicalPass = physicalPasses[lastUsedPass];
                if (i + 1 < (int)chain.size())
                    physicalPass.aliasTransfers.push_back(std::make_pair(chain[i], chain[i + 1]));
                else
                    physicalPass.aliasTransfers.push_back(std::make_pair(chain[i], chain[0]));
            }
        }
    }

    void RenderGraphImpl::SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain)
    {
        // Build physical attachments/buffers from physical dimensions
        physicalAttachments.clear();
        physicalAttachments.resize(physicalDimensions.size());

        // Try to reuse
        physicalEvents.resize(physicalDimensions.size());
        physicalBuffers.resize(physicalDimensions.size());
        physicalImages.resize(physicalDimensions.size());

        // Func to create new image
        auto SetupPhysicalImage = [&](U32 attachment) {
        
            // Check alias
            if (physicalAliases[attachment] != RenderResource::Unused)
            {
                physicalImages[attachment] = physicalImages[physicalAliases[attachment]];
                physicalAttachments[attachment] = &physicalImages[attachment]->GetImageView();
                return;
            }

            auto& physicalDim = physicalDimensions[attachment];
            bool needToCreate = true;
            VkImageUsageFlags usage = physicalDim.imageUsage;
            VkImageCreateFlags flags = 0;

            // Check previous image cache is same to new
            if (physicalImages[attachment])
            {
                auto& imgInfo = physicalImages[attachment]->GetCreateInfo();
                if ((imgInfo.width == physicalDim.width) &&
                    (imgInfo.height == physicalDim.height) &&
                    (imgInfo.format == physicalDim.format) &&
                    (imgInfo.depth == physicalDim.depth) &&
                    (imgInfo.samples == physicalDim.samples) &&
                    ((imgInfo.usage & usage) == usage) &&
                    ((imgInfo.flags & flags) == flags))
                    needToCreate = false;
            }

            if (needToCreate)
            {
                GPU::ImageCreateInfo info = {};
                info.width = physicalDim.width;
                info.height = physicalDim.height;
                info.depth = physicalDim.depth;
                info.format = physicalDim.format;
                info.levels = physicalDim.levels;
                info.layers = physicalDim.layers;
                info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                info.samples = (VkSampleCountFlagBits)physicalDim.samples;
                info.domain = GPU::ImageDomain::Physical;
                info.usage = usage;
                info.flags = flags;

                if (GPU::IsFormatHasDepthOrStencil(info.format))
                    info.usage &= ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

                info.misc = 0;
                if (physicalDim.queues & ((U32)RenderGraphQueueFlag::Graphics | (U32)RenderGraphQueueFlag::Compute))
                    info.misc |= GPU::IMAGE_MISC_CONCURRENT_QUEUE_GRAPHICS_BIT;
                if (physicalDim.queues & (U32)RenderGraphQueueFlag::AsyncCompute)
                    info.misc |= GPU::IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_COMPUTE_BIT;
                if (physicalDim.queues & (U32)RenderGraphQueueFlag::AsyncGraphcs)
                    info.misc |= GPU::IMAGE_MISC_CONCURRENT_QUEUE_ASYNC_GRAPHICS_BIT;

                physicalImages[attachment] = device.CreateImage(info, nullptr);
                if (!physicalImages[attachment])
                    Logger::Error("Faile to create image of render graph.");

                if (physicalDim.IsStorageImage())
                    physicalImages[attachment]->SetLayoutType(GPU::ImageLayoutType::General);

                device.SetName(*physicalImages[attachment], physicalDim.name);
            }

            physicalAttachments[attachment] = &physicalImages[attachment]->GetImageView();
        };

        // Func to create new buffer
        auto SetupPhysicalBuffers = [&](U32 attachment) {
            auto& physicalDim = physicalDimensions[attachment];
            bool needToCreate = true;
            VkBufferUsageFlags usage = physicalDim.bufferInfo.usage;

            if (physicalBuffers[attachment])
            {
                auto& bufferInfo = physicalBuffers[attachment]->GetCreateInfo();
                if ( bufferInfo.size == physicalDim.bufferInfo.size &&
                    ((bufferInfo.usage & usage) == usage))
                    needToCreate = false;
            }

            if (needToCreate)
            {
                GPU::BufferCreateInfo info = {};
                info.domain = GPU::BufferDomain::Device;
                info.size = physicalDim.bufferInfo.size;
                info.usage = usage;
                info.misc = GPU::BUFFER_MISC_ZERO_INITIALIZE_BIT;

                physicalBuffers[attachment] = device.CreateBuffer(info, nullptr);
                if (!physicalImages[attachment])
                    Logger::Error("Faile to create buffer of render graph.");
            }
        };

        for (int i = 0; i < physicalDimensions.size(); i++)
        {
            auto& physicalDim = physicalDimensions[i];
            if (physicalDim.IsBuffer())
            {
                SetupPhysicalBuffers(i);
            }
            else if (physicalDim.IsStorageImage())
            {
                SetupPhysicalImage(i);
            }
            else if (i == swapchainPhysicalIndex)
            {
                physicalAttachments[i] = swapchain;
            }
            else
            {
                if (physicalDim.isTransient)
                {
                    physicalImages[i] = device.RequestTransientAttachment(
                        physicalDim.width, physicalDim.height, physicalDim.format, i, physicalDim.samples, physicalDim.layers);
                    physicalAttachments[i] = &physicalImages[i]->GetImageView();
                }
                else
                {
                    SetupPhysicalImage(i);
                }
            }
        }

        // Assign physical attachments to PhysicalPass::GPURenderPassInfo
        for (auto& physicalPass : physicalPasses)
        {
            auto& gpuRenderPassInfo = physicalPass.gpuRenderPassInfo;
            U32 numColorAttachments = physicalPass.physicalColorAttachments.size();
            for (U32 i = 0; i < numColorAttachments; i++)
            {
                gpuRenderPassInfo.colorAttachments[i] = 
                    physicalAttachments[physicalPass.physicalColorAttachments[i]];
            }
        }
    }

    void RenderGraphImpl::HandleInvalidateBarrier(const Barrier& barrier, GPUPassSubmissionState& state, bool isGraphicsQueue)
    {
        bool needEventBarrier = false;
        bool needSempahore = false;

        auto& ent = physicalEvents[barrier.resIndex];
        auto& physicalRes = physicalDimensions[barrier.resIndex];
        if (physicalRes.IsBuffer())
        {
        }
        else
        {
            GPU::Image* image = physicalImages[barrier.resIndex].get();
            if (image == nullptr)
                return;

            // Create image memory barrier
            VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            b.image = image->GetImage();
            b.oldLayout = ent.imageLayout;
            b.newLayout = barrier.layout;
            b.srcAccessMask = ent.flushAccess;
            b.dstAccessMask = barrier.access;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.subresourceRange.levelCount = image->GetCreateInfo().levels;
            b.subresourceRange.layerCount = image->GetCreateInfo().layers;
            b.subresourceRange.aspectMask = GPU::formatToAspectMask(image->GetCreateInfo().format);
            
            // Update image layout of event
            ent.imageLayout = barrier.layout;

            bool layoutChange = b.oldLayout != b.newLayout;
            bool needSync = layoutChange || (ent.flushAccess != 0);
            if (needSync)
            {
                if (ent.ent)
                {
                    state.eventImageBarriers.push_back(b);
                    needEventBarrier = true;
                }
                else
                {
                    GPU::SemaphorePtr waitSemaphore = isGraphicsQueue ? ent.waitGraphicsSemaphore : ent.waitComputeSemaphore;
                    if (waitSemaphore)
                    {
                        if (layoutChange)
                        {
                            b.srcAccessMask = 0;
                            state.handoverBarriers.push_back(b);
                            state.handoverStages |= barrier.stages;
                        }
                        needSempahore = true;
                    }
                    else
                    {
                        // Is the first time to use the resource
                        ASSERT(b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
                        state.immediateImageBarriers.push_back(b);
                        state.immediateDstStages |= barrier.stages;
                    }
                }
            }
        }

        if (needEventBarrier)
        {
            ASSERT(ent.ent);
            state.srcStages |= ent.ent->GetStages();
            state.dstStages |= barrier.stages;

            auto it = std::find(state.events.begin(), state.events.end(), ent.ent->GetEvent());
            if (it == state.events.end())
                state.events.push_back(ent.ent->GetEvent());
        }
        else if (needSempahore)
        {
            GPU::SemaphorePtr waitSemaphore = isGraphicsQueue ? ent.waitGraphicsSemaphore : ent.waitComputeSemaphore;
            ASSERT(waitSemaphore);
            ASSERT(waitSemaphore->IsSignalled());

            state.waitSemaphores.push_back(waitSemaphore);
            state.waitSemaphoreStages.push_back(barrier.stages);
        }
    }
    
    void RenderGraphImpl::HandleFlushBarrier(const Barrier& barrier, GPUPassSubmissionState& state)
    {
        auto& ent = physicalEvents[barrier.resIndex];
        auto& physicalRes = physicalDimensions[barrier.resIndex];
        if (!physicalRes.IsBuffer())
        {
            GPU::Image* image = physicalImages[barrier.resIndex].get();
            if (image == nullptr)
                return;

            ent.imageLayout = barrier.layout;
        }

        ent.flushAccess = barrier.access;

        if (physicalRes.UseSemaphore())
        {
            ent.waitGraphicsSemaphore = state.graphicsSemaphore;
            ent.waitComputeSemaphore = state.computeSemaphore;
        }
        else
        {
            ASSERT(state.signalEvent);
            ent.ent = state.signalEvent;
        }
    }

    void RenderGraphImpl::DoGraphicsCommands(GPU::CommandList& cmd, PhysicalPass& physicalPass, GPUPassSubmissionState* state)
    {
        // Clear colors
        for (auto& colorClear : physicalPass.colorClearRequests)
            colorClear.pass->GetClearColor(colorClear.index, colorClear.target);

        // Clear depth stencil
        if (physicalPass.depthClearRequest.target != nullptr)
            physicalPass.depthClearRequest.pass->GetClearDepthStencil(physicalPass.depthClearRequest.target);

        // Begin render pass
        cmd.BeginEvent("BeginRenderPass");
        cmd.BeginRenderPass(physicalPass.gpuRenderPassInfo);
        cmd.EndEvent();

        // Handle subpasses
        for (U32 i = 0; i < physicalPass.passes.size(); i++)
        {
            auto& passIndex = physicalPass.passes[i];
            auto& pass = *renderPasses[passIndex];
            cmd.BeginEvent("DoRenderPass");
            pass.BuildRenderPass(cmd);
            cmd.EndEvent();

            if (i < (physicalPass.passes.size() - 1))
                cmd.NextSubpass(VK_SUBPASS_CONTENTS_INLINE);
        }

        // End render pass
        cmd.BeginEvent("EndRenderPass");
        cmd.EndRenderPass();
        cmd.EndEvent();
    }

    void RenderGraphImpl::DoComputeCommands(GPU::CommandList& cmd, PhysicalPass& physicalPass, GPUPassSubmissionState* state)
    {
    }

    void RenderGraphImpl::Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle)
    {
        submissionStates.clear();
        submissionStates.resize(physicalPasses.size());

        // Traverse physical passes to build GPUSubmissionInfos
        for (int i = 0; i < physicalPasses.size(); i++)
        {
            auto& physicalPass = physicalPasses[i];
            if (physicalPass.passes.empty())
                continue;

            auto& state = submissionStates[i];
#ifdef DEBUG
            state.name = renderPasses[physicalPass.passes[0]]->GetName().c_str();
#endif
            U32 queueFlag = renderPasses[physicalPass.passes[0]]->GetQueue();
            switch (queueFlag)
            {
            case (U32)RenderGraphQueueFlag::Graphics:
                state.isGraphics = true;
                state.queueType = GPU::QueueType::QUEUE_TYPE_GRAPHICS;
                break;

            case (U32)RenderGraphQueueFlag::Compute:
                state.isGraphics = false;
                state.queueType = GPU::QueueType::QUEUE_TYPE_GRAPHICS;
                break;

            case (U32)RenderGraphQueueFlag::AsyncCompute:
                state.isGraphics = false;
                state.queueType = GPU::QueueType::QUEUE_TYPE_ASYNC_COMPUTE;
                break;

            default:
                ASSERT(0);
                break;
            }

            // Handle discard
            for (auto& discard : physicalPass.discards)
            {
                if (!physicalDimensions[discard].IsBufferLikeRes())
                    physicalEvents[discard].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            // Handle invalidate barriers
            for (auto& barrier : physicalPass.invalidate)
                HandleInvalidateBarrier(barrier, state, state.isGraphics);

            // Create signal events and semaphores (for multi queues)
            for (auto& barrier : physicalPass.flush)
            {
                auto& physicalDim = physicalDimensions[barrier.resIndex];
                if (physicalDim.UseSemaphore())
                    state.needSubmissionSemaphore = true;
                else
                    state.signalEventStages |= barrier.stages;
            }
            if (state.signalEventStages != 0)
                state.signalEvent = device.RequestSignalEvent(state.signalEventStages);
                
            if (state.needSubmissionSemaphore)
            {
                state.graphicsSemaphore = device.RequestEmptySemaphore();
                state.computeSemaphore = device.RequestEmptySemaphore();
            }

            // Handle flush barriers
            for (auto& barrier : physicalPass.flush)
                HandleFlushBarrier(barrier, state);

            // Handle alias transfer of physical pass
            for (auto& transfer : physicalPass.aliasTransfers)
            {
                // Wait on the event before we transfer to alias
                auto& ent = physicalEvents[transfer.second];
                ent = physicalEvents[transfer.first];
                ent.flushAccess = 0;
                ent.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }

            state.active = true;
        }

        // Do gpu behaviors, could be run in parallel.
        for (U32 i = 0; i < submissionStates.size(); i++)
        {
            auto& state = submissionStates[i];
            if (!state.active)
                continue;

            ASSERT(state.renderingDependency.counter == 0);

            auto& physicalPass = physicalPasses[i];
            Jobsystem::Run(&state, [this, &device, &physicalPass](void* data)->void {
                GPUPassSubmissionState* state = (GPUPassSubmissionState*)data;
                if (state == nullptr)
                    return;

                GPU::CommandListPtr cmd = device.RequestCommandList(state->queueType);
                if (!cmd)
                {
                    Logger::Error("Failed to create command list for render graph");
                    return;
                }
                state->cmd = cmd;

                state->EmitPrePassBarriers();

                if (state->isGraphics)
                    DoGraphicsCommands(*cmd, physicalPass, state);
                else
                    DoComputeCommands(*cmd, physicalPass, state);

                state->EmitPostPassBarriers();

                // We end this cmd on a same thread we requested it on
                cmd->EndCommandBufferForThread();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
                Logger::Print("Pass %s execute", state->name);
#endif

            }, & state.renderingDependency);

            ASSERT(state.renderingDependency.counter > 0);
        }

        // Sequential submit all states
        Jobsystem::JobHandle* lastSubmissionHandle = nullptr;
        for (auto& state : submissionStates)
        {
            if (state.active == false)
                continue;

            Jobsystem::JobHandle* renderingDependency = nullptr;
            if (state.renderingDependency)
                renderingDependency = &state.renderingDependency;
          
            Jobsystem::Run(&state, [lastSubmissionHandle, renderingDependency](void* data)->void {
                // Wait previous submission
                Jobsystem::Wait(lastSubmissionHandle);
                // Wait GPU behavior
                Jobsystem::Wait(renderingDependency);

                GPUPassSubmissionState* state = (GPUPassSubmissionState*)data;
                if (state == nullptr || !state->cmd)
                    return;

                state->Submit();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
                Logger::Print("Pass %s submit", state->name);
#endif
            }, &state.submissionHandle);

            lastSubmissionHandle = &state.submissionHandle;
        }

        // Flush swapchain
        Jobsystem::Run(nullptr, [&device, lastSubmissionHandle](void* data)->void {
            Jobsystem::Wait(lastSubmissionHandle);
            device.FlushFrames();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
            Logger::Print("FlushFrames");
#endif
        }, &jobHandle);
    }

    void RenderGraphImpl::Log()
    {
        Logger::Info("----------------------------------------------------");
        Logger::Info("RenderGraph");
        Logger::Info("----------------------------------------------------");
        // Resources
        Logger::Info("Resource:");
        for (auto& res : physicalDimensions)
        {
            if (res.bufferInfo.size)
            {
                Logger::Info("Resource #%u (%s): size: %u",
                    U32(&res - physicalDimensions.data()),
                    res.name.c_str(),
                    U32(res.bufferInfo.size));
            }
            else
            {
                Logger::Info("Resource #%u (%s): %u x %u (fmt: %u), samples: %u, transient: %s%s",
                    U32(&res - physicalDimensions.data()),
                    res.name.c_str(),
                    res.width, res.height, unsigned(res.format), res.samples,
                    res.isTransient  ? "yes" : "no",
                    U32(&res - physicalDimensions.data()) == swapchainPhysicalIndex ? " (swapchain)" : "");
            }
        }

        auto IsSwapChainString = [this](const Barrier& barrier) {
            return barrier.resIndex == swapchainPhysicalIndex ? " (swapchain)" : "";
        };

        
        U32 passBarrierIndex = 0;
        Logger::Info("Render passes:");
        for (auto& pass : physicalPasses)
        {
            Logger::Info("Physical pass #%u:", unsigned(&pass - physicalPasses.data()));

            // Invalidates
            for (auto& barrier : pass.invalidate)
            {
                Logger::Info("  Invalidate: %u%s, layout: %s, access: %s, stages: %s",
                    barrier.resIndex,
                    IsSwapChainString(barrier),
                    GPU::LayoutToString(barrier.layout),
                    GPU::AccessFlagsToString(barrier.access).c_str(),
                    GPU::StageFlagsToString(barrier.stages).c_str());
            }

            // Subpasses
            for (auto& subpass : pass.passes)
            {
                Logger::Info("    Subpass #%u (%s):", unsigned(&subpass - pass.passes.data()), renderPasses[subpass]->GetName().c_str());
                RenderPass& renderPass = *renderPasses[subpass];
                PassBarrier& passBarrier = passBarriers[passBarrierIndex];
                // Invalidate
                for (auto& barrier : passBarrier.invalidate)
                {
                    if (!physicalDimensions[barrier.resIndex].isTransient)
                    {
                        Logger::Info("      Invalidate: %u%s, layout: %s, access: %s, stages: %s",
                            barrier.resIndex,
                            IsSwapChainString(barrier),
                            GPU::LayoutToString(barrier.layout),
                            GPU::AccessFlagsToString(barrier.access).c_str(),
                            GPU::StageFlagsToString(barrier.stages).c_str());
                    }
                }

                // Flush
                for (auto& barrier : passBarrier.flush)
                {
                    if (!physicalDimensions[barrier.resIndex].isTransient && barrier.resIndex != swapchainPhysicalIndex)
                    {
                        Logger::Info("      Flush: %u, layout: %s, access: %s, stages: %s",
                            barrier.resIndex,
                            GPU::LayoutToString(barrier.layout),
                            GPU::AccessFlagsToString(barrier.access).c_str(),
                            GPU::StageFlagsToString(barrier.stages).c_str());
                    }
                }

                passBarrierIndex++;
            }

            // Flush
            for (auto& barrier : pass.flush)
            {
                Logger::Info("  Flush: %u%s, layout: %s, access: %s, stages: %s",
                    barrier.resIndex,
                    IsSwapChainString(barrier),
                    GPU::LayoutToString(barrier.layout),
                    GPU::AccessFlagsToString(barrier.access).c_str(),
                    GPU::StageFlagsToString(barrier.stages).c_str());
            }
        }

        Logger::Info("----------------------------------------------------");
    }

    String RenderGraphImpl::DebugExportGraphviz()
    {
        String ret;
        ret += "digraph \"graph\" {\n";
        ret += "rankdir = LR\n";
        ret += "bgcolor = black\n";
        ret += "node [shape=rectangle, fontname=\"helvetica\", fontsize=10]\n\n";
    
        // Physical passes
        for (auto& physicalPass : physicalPasses)
        {
            for (U32 passIndex : physicalPass.passes)
            {
                auto& pass = renderPasses[passIndex];
                std::string passIndexString = std::to_string(passIndex).c_str();

                ret += "Pass_";
                ret += passIndexString.c_str();
                ret += " [label=\"";
                ret += pass->GetName();
                ret += ", id: ";
                ret += passIndexString.c_str();
                ret += "\", ";
                ret += "style=filled, fillcolor=";
                ret += pass->GetPhysicalIndex() != RenderPass::Unused ? "darkorange" : "darkorange4";
                ret += "]\n";
            }
        }

        // Physical resources
        for (U32 resIndex = 0; resIndex < physicalDimensions.size(); resIndex++)
        {
            auto& res = physicalDimensions[resIndex];
            std::string resIndexString = std::to_string(resIndex).c_str();

            ret += "Res_";
            ret += resIndexString.c_str();
            ret += " [label=\"";
            ret += res.name;
            ret += ", id: ";
            ret += resIndexString.c_str();
            ret += "\", ";
            ret += "style=filled, fillcolor=";
            ret += "skyblue";
            ret += "]\n";
        }

        // Add Edge
        for (auto& physicalPass : physicalPasses)
        {
            for (U32 passIndex : physicalPass.passes)
            {
                auto& pass = renderPasses[passIndex];
                // InputTextures
                for (auto& tex : pass->GetInputTextures())
                {
                    auto& res = tex.texture;
                    U32 physicalIndex = res->GetPhysicalIndex();
                    if (physicalIndex != RenderResource::Unused)
                    {      
                        ret += "Res_";
                        ret += std::to_string(physicalIndex).c_str();
                        ret += "-> ";
                        ret += "Pass_";
                        ret += std::to_string(passIndex).c_str();
                        ret += " [color=red2]\n";
                    }
                }
                // Outputs
                for (auto& res : pass->GetOutputColors())
                {
                    U32 physicalIndex = res->GetPhysicalIndex();
                    if (physicalIndex != RenderResource::Unused)
                    {
                        ret += "Pass_";
                        ret += std::to_string(passIndex).c_str();
                        ret += "-> ";
                        ret += "Res_";
                        ret += std::to_string(physicalIndex).c_str();
                        ret += " [color=red2]\n";
                    }
                }
            }
        }

        ret += "}\n";
        ret += "----------------------------------------------------";
        Logger::Print(ret);
        return ret;
    }

    void GPUPassSubmissionState::EmitPrePassBarriers()
    {
        cmd->BeginEvent("RenderGraphSyncPre");

        // Barriers
        if (!immediateImageBarriers.empty() || !handoverBarriers.empty())
        {
            std::vector<VkImageMemoryBarrier> combinedBarriers;
            combinedBarriers.reserve(handoverBarriers.size() + immediateImageBarriers.size());
            combinedBarriers.insert(combinedBarriers.end(), handoverBarriers.begin(), handoverBarriers.end());
            combinedBarriers.insert(combinedBarriers.end(), immediateImageBarriers.begin(), immediateImageBarriers.end());
            
            VkPipelineStageFlags src = handoverStages;
            VkPipelineStageFlags dst = handoverStages | immediateDstStages;
            if (src == 0)
                src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            cmd->Barrier(src, dst, 
                combinedBarriers.size(), combinedBarriers.empty() ? nullptr : combinedBarriers.data());
        }

        // Wait events
        if (!eventImageBarriers.empty() || !eventBufferBarriers.empty())
        {
            cmd->WaitEvents(
                events.size(), events.data(),
                srcStages, dstStages,
                0, nullptr, 
                eventBufferBarriers.size(), eventBufferBarriers.empty() ? nullptr : eventBufferBarriers.data(),
                eventImageBarriers.size(), eventImageBarriers.empty() ? nullptr : eventImageBarriers.data()
            );
        }

        cmd->EndEvent();
    }

    void GPUPassSubmissionState::EmitPostPassBarriers()
    {
        cmd->BeginEvent("RenderGraphSyncPost");
        if (signalEventStages != 0)
            cmd->CompleteEvent(*signalEvent);
        cmd->EndEvent();
    }

    void GPUPassSubmissionState::Submit()
    {
        if (!cmd) return;

        auto& device = cmd->GetDevice();
        
        // Wait semaphores in queue
        for (U32 i = 0; i < waitSemaphores.size(); i++)
        {
            auto& semaphore = waitSemaphores[i];
            if (semaphore->GetSemaphore() != VK_NULL_HANDLE && !semaphore->IsPendingWait())
                device.AddWaitSemaphore(queueType, semaphore, waitSemaphoreStages[i], true);
        }

        if (needSubmissionSemaphore)
        {
            GPU::SemaphorePtr semaphores[2];
            device.Submit(cmd, nullptr, 2, semaphores);

            *graphicsSemaphore = std::move(*semaphores[0]);
            *computeSemaphore = std::move(*semaphores[1]);
        }
        else
        {
            device.Submit(cmd);
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

        impl->passDependency.clear();
        impl->passDependency.resize(impl->renderPasses.size());

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
        bool canAliasBackbuffer = (backbufferDim.queues & (U32)RenderGraphQueueFlag::Compute) == 0 && backbufferDim.isTransient;
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

        // Build aliases
        impl->BuildAliases();
    }

    void RenderGraph::SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain)
    {
        impl->SetupAttachments(device, swapchain);
    }
    
    void RenderGraph::Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle)
    {
        impl->Render(device, jobHandle);
    }
    
    void RenderGraph::Log()
    {
        impl->Log();
        impl->DebugExportGraphviz();
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

    GPU::ImageView& RenderGraph::GetPhysicalTexture(const RenderTextureResource& res)
    {
        ASSERT(res.GetPhysicalIndex() != RenderResource::Unused);
        return *impl->physicalAttachments[res.GetPhysicalIndex()];
    }

    void RenderGraph::SetBackbufferDimension(const ResourceDimensions& dim)
    {
        impl->swapchainDimensions = dim;
    }

}
