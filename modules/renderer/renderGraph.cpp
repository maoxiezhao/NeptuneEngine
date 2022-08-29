#include "renderer.h"
#include "renderGraph.h"
#include "gpu\vulkan\typeToString.h"
#include "core\memory\memory.h"
#include "core\jobsystem\jobsystem.h"

#include <stdexcept>
#include <stack>

namespace VulkanTest
{
#define RENDER_GRAPH_LOGGING_LEVEL (0)

    static const U32 COMPUTE_QUEUES =
        (U32)RenderGraphQueueFlag::AsyncCompute |
        (U32)RenderGraphQueueFlag::Compute;

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
        GPU::RenderPassInfo renderPassInfo = {};
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
        VkPipelineStageFlags pieplineBarrierSrcStages = 0;
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
        std::vector<VkBufferMemoryBarrier> bufferBarriers;
        std::vector<VkImageMemoryBarrier> imageBarriers;

        // Immediate buffer barriers are useless because they don't need any layout transition.
        // Immediate image barriers are purely for doing layout transitions without waiting (srcStage = TOP_OF_PIPE).
        std::vector<VkImageMemoryBarrier> immediateImageBarriers;

        // Barriers which are used when waiting for a semaphore, and then doing a transition.
        std::vector<VkImageMemoryBarrier> handoverBarriers;

        std::vector<GPU::SemaphorePtr> waitSemaphores;
        std::vector<VkPipelineStageFlags> waitSemaphoreStages;

        VkPipelineStageFlags postPipelineBarrierStages;
        VkPipelineStageFlags preSrcStages = 0;
        VkPipelineStageFlags preDstStages = 0;
        VkPipelineStageFlags immediateDstStages = 0;
        VkPipelineStageFlags handoverStages = 0;

        GPU::SemaphorePtr graphicsSemaphore;
        GPU::SemaphorePtr computeSemaphore;

        std::vector<VkSubpassContents> subpassContents;

        Jobsystem::JobHandle renderingDependency;
        Jobsystem::JobHandle submissionHandle;

        bool needSubmissionSemaphore = false;

        void EmitPrePassBarriers();
        void Submit();
    };

    ///////////////////////////////////////////////////////////////////////////////////
    
    struct RenderGraphImpl
    {
        GPU::DeviceVulkan* device;
        String backbufferSource;
        bool swapchainEnable = true;
        bool isBaked = false;

        std::unordered_map<String, U32> nameToRenderPassIndex;
        std::vector<UniquePtr<RenderPass>> renderPasses;
        std::unordered_map<String, U32> nameToResourceIndex;
        std::vector<UniquePtr<RenderResource>> resources;

        std::vector<U32> passStack;
        std::vector<std::unordered_set<U32>> passDependency;
        std::vector<PassBarrier> passBarriers;
        Jobsystem::JobHandle submitHandle;

        GPU::ImageView* swapchainAttachment;
        VkImageLayout swapchainLayout;

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
        void SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain, VkImageLayout finalLayout);
        void HandleInvalidateBarrier(const Barrier& barrier, GPUPassSubmissionState& state, bool isGraphicsQueue);
        void HandleSignal(const PhysicalPass& physicalPass, GPUPassSubmissionState& state);
        void HandleFlushBarrier(const Barrier& barrier, GPUPassSubmissionState& state);  
        void DoGraphicsCommands(GPU::CommandList& cmd, const PhysicalPass& physicalPass, GPUPassSubmissionState* state);
        void DoComputeCommands(GPU::CommandList& cmd, const PhysicalPass& physicalPass, GPUPassSubmissionState* state);
        void TransferOwnership(const PhysicalPass& physicalPass);
        void HandleTimelineGPU(GPU::DeviceVulkan& device, const PhysicalPass& physicalPass, GPUPassSubmissionState* state, U8 index);
        void EnqueueRenderPass(PhysicalPass& physicalPass, GPUPassSubmissionState& state);
        void SwapchainLayoutTransitionPass();
        void Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle);
        void Reset();

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

    void RenderGraphImpl::TraverseDependencies(RenderPass& pass, U32 stackCount)
    {
#if 0
        for (auto* input : pass.GetInputColors())
        {
            if (input != nullptr)
                HandlePassRecursive(pass, input->GetWrittenPasses(), stackCount);
        }
#endif

        if (pass.GetInputDepthStencil())
        {
            HandlePassRecursive(pass, pass.GetInputDepthStencil()->GetWrittenPasses(), stackCount);
        }

        for (auto& res : pass.GetInputTextures())
        {
            if (res.texture != nullptr)
                HandlePassRecursive(pass, res.texture->GetWrittenPasses(), stackCount);
        }

        for (auto& input : pass.GetProxyInputs())
            HandlePassRecursive(pass, input.proxy->GetWrittenPasses(), stackCount);

        for (auto& input : pass.GetInputStorageTextures())
        {
            if (input != nullptr)
                HandlePassRecursive(pass, input->GetWrittenPasses(), stackCount);
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

            if (!pass->GetInputColors().empty())
            {
                auto& inputColors = pass->GetInputColors();
                for (int i = 0; i < inputColors.size(); i++)
                {
                    auto* input = inputColors[i];
                    if (input != nullptr)
                    {
                        AddImagePhysicalDimension(input);

                        if (pass->GetOutputColors()[i]->GetPhysicalIndex() == RenderResource::Unused)
                            pass->GetOutputColors()[i]->SetPhysicalIndex(input->GetPhysicalIndex());
                        else if (pass->GetOutputColors()[i]->GetPhysicalIndex() != input->GetPhysicalIndex())
                            Logger::Error("Cannot alias resources. Index already claimed.");
                    }
                }
            }

            if (!pass->GetInputStorageTextures().empty())
            {
                auto& textures = pass->GetInputStorageTextures();
                for (int i = 0; i < textures.size(); i++)
                {
                    auto* input = textures[i];
                    if (input != nullptr)
                    {
                        AddImagePhysicalDimension(input);

                        if (pass->GetOutputStorageTextures()[i]->GetPhysicalIndex() == RenderResource::Unused)
                            pass->GetOutputStorageTextures()[i]->SetPhysicalIndex(input->GetPhysicalIndex());
                        else if (pass->GetOutputStorageTextures()[i]->GetPhysicalIndex() != input->GetPhysicalIndex())
                            Logger::Error("Cannot alias resources. Index already claimed.");
                    }
                }
            }

            for (auto& output : pass->GetOutputColors())
                AddImagePhysicalDimension(output);

            for (auto& output : pass->GetOutputStorageTextures())
                AddImagePhysicalDimension(output);

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

            for (auto& pair : pass->GetFakeResourceAliases())
                pair.second->SetPhysicalIndex(pair.first->GetPhysicalIndex());
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
                if (FindTexture(prevPass.GetOutputStorageTextures(), input.texture))
                    return false;
                if (prevPass.GetOutputDepthStencil() == input.texture)
                    return false;
            }
            for (auto& input : nextPass.GetInputStorageBuffers())
            {
                if (FindBuffer(prevPass.GetOutputStorageBuffers(), input))
                    return false;
            }

            for (auto& input : nextPass.GetInputStorageTextures())
            {
                if (FindTexture(prevPass.GetOutputStorageTextures(), input))
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

            for (auto* input : nextPass.GetInputColors())
            {
                if (input == nullptr)
                    continue;

                if (FindTexture(prevPass.GetOutputStorageTextures(), input))
                    return false;

                if (FindTexture(prevPass.GetOutputColors(), input))
                    return true;
            }

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
            if (res->GetResourceType() != RenderGraphResourceType::Texture)
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
            auto& renderPassInfo = physicalPass.renderPassInfo;
            renderPassInfo.numSubPasses = (U32)physicalPass.gpuSubPasses.size();
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
                subPass.numColorAattachments = (U32)outputColors.size();
                for (U32 i = 0; i < subPass.numColorAattachments; i++)
                {
                    auto ret = AddColorAttachment(outputColors[i]->GetPhysicalIndex());
                    subPass.colorAttachments[i] = ret.first;
                    if (ret.second)
                    {
                        // This is the first time to use color attachment
                        bool hasColorInput = !pass.GetInputColors().empty() && pass.GetInputColors()[i] != nullptr;
                        if (hasColorInput)
                            renderPassInfo.loadAttachments |= 1u << ret.first;
           
                        if (pass.GetClearColor(i))
                        {
                            renderPassInfo.clearAttachments |= 1u << ret.first;
                            physicalPass.colorClearRequests.push_back({
                                &pass,
                                &renderPassInfo.clearColor[ret.first],
                                (U32)i });
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

                        bool preserveDepth = CheckPreserve(*dsInput);
                        if (!preserveDepth)
                        {
                            for (auto& logicalPass : renderPasses)
                            {
                                for (auto& alias : logicalPass->GetFakeResourceAliases())
                                {
                                    if (alias.first == dsInput && CheckPreserve(*alias.second))
                                    {
                                        preserveDepth = true;
                                        break;
                                    }
                                }
                            }
                        }

                        if (preserveDepth)
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
                subPass.numInputAttachments = (U32)inputAttachments.size();
                for (U32 i = 0; i < subPass.numInputAttachments; i++)
                {     
                    auto ret = AddInputAttachment(inputAttachments[i]->GetPhysicalIndex());
                    subPass.inputAttachments[i] = ret.first;
                    if (ret.second)
                        renderPassInfo.loadAttachments |= 1u << ret.first;
                }
            }
            
            renderPassInfo.numColorAttachments = (U32)physicalPass.physicalColorAttachments.size();
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

            for (auto* input : pass.GetInputColors())
            {
                if (input == nullptr)
                    continue;

                if (pass.GetQueue() & COMPUTE_QUEUES)
                    throw std::logic_error("Only graphics passes can have color inputs.");

                Barrier& barrier = GetInvalidateAccess(input->GetPhysicalIndex());
                barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                else if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
                    ASSERT_MSG(false, "Layout mismatch.");
                else
                    barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            for (auto* input : pass.GetInputStorageTextures())
            {
                if (input == nullptr)
                    continue;

                Barrier& barrier = GetInvalidateAccess(input->GetPhysicalIndex());
                barrier.access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                if (pass.GetQueue() & COMPUTE_QUEUES)
                    barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                else
                    barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
                    ASSERT_MSG(false, "Layout mismatch.");
                barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
            }

            /////////////////////////////////////////////////////////////////////
            // Output
            // Push into flush list
            auto GetFlushAccess = [&](U32 index)->Barrier& {
                return GetAccessBarrier(passBarrier.flush, index);
            };
            for (auto& color : pass.GetOutputColors())
            {
                if (pass.GetQueue() & COMPUTE_QUEUES)
                    throw std::logic_error("Only graphics passes can have color outputs.");

                Barrier& barrier = GetFlushAccess(color->GetPhysicalIndex());
                barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                barrier.stages |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                // Set VK_IMAGE_LAYOUT_GENERAL if the attachment is also bound as an input attachment(ReadTexture)
                if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
                    barrier.layout == VK_IMAGE_LAYOUT_GENERAL)
                {
                    barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
                }
                else 
                {
                    ASSERT(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED);
                    barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }

            for (auto& output : pass.GetOutputStorageTextures())
            {
                Barrier& barrier = GetFlushAccess(output->GetPhysicalIndex());
                barrier.access |= VK_ACCESS_SHADER_WRITE_BIT;
                if (pass.GetQueue() & COMPUTE_QUEUES)
                    barrier.stages |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
                else
                    barrier.stages |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                if (barrier.layout != VK_IMAGE_LAYOUT_UNDEFINED)
                    ASSERT_MSG(false, "Layout mismatch.");
                barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
            }

            /////////////////////////////////////////////////////////////////////
            // Depth stencil
            auto* dsInput = pass.GetInputDepthStencil();
            auto* dsOutput = pass.GetOutputDepthStencil();

            if ((dsInput || dsOutput) && (pass.GetQueue() & COMPUTE_QUEUES))
                throw std::logic_error("Only graphics passes can have dsInput or dsOutput.");

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

        auto barrierIt = passBarriers.begin();
        for (auto& physicalPass : physicalPasses)
        {
            resourceStates.clear();
            resourceStates.resize(physicalDimensions.size());

            // In multipass, only the first and last barriers need to be considered externally.
            for (U32 i = 0; i < physicalPass.passes.size(); i++, ++barrierIt)
            {
                PassBarrier& passBarrier = *barrierIt;
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
                        if (flush.layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                        {
                            resState.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                            resState.invalidatedStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                            resState.invalidatedTypes = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                        }
                        else
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
                        }

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
            for (auto& input : pass->GetInputColors())
                AddReaderPass(input, pass->GetPhysicalIndex());
            for (auto& input : pass->GetInputStorageTextures())
                AddReaderPass(input, pass->GetPhysicalIndex());
            if (pass->GetInputDepthStencil())
                AddReaderPass(pass->GetInputDepthStencil(), pass->GetPhysicalIndex());

            for (auto& output : pass->GetOutputColors())
                AddWriterPass(output, pass->GetPhysicalIndex());
            for (auto& output : pass->GetOutputStorageTextures())
                AddWriterPass(output, pass->GetPhysicalIndex());
            if (pass->GetOutputDepthStencil())
                AddWriterPass(pass->GetOutputDepthStencil(), pass->GetPhysicalIndex());
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

    void RenderGraphImpl::SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain, VkImageLayout finalLayout)
    {
        // Build physical attachments/buffers from physical dimensions
        physicalAttachments.clear();
        physicalAttachments.resize(physicalDimensions.size());

        // Try to reuse
        physicalEvents.resize(physicalDimensions.size());
        physicalBuffers.resize(physicalDimensions.size());
        physicalImages.resize(physicalDimensions.size());

        swapchainAttachment = swapchain;
        swapchainLayout = finalLayout;

        // Func to create new image
        auto SetupPhysicalImage = [&](U32 attachment) {
        
            // Check alias
            if (physicalAliases[attachment] != RenderResource::Unused)
            {
                physicalImages[attachment] = physicalImages[physicalAliases[attachment]];
                physicalAttachments[attachment] = &physicalImages[attachment]->GetImageView();
                physicalEvents[attachment] = {};
                return;
            }

            bool needToCreate = true;

            auto& physicalDim = physicalDimensions[attachment];
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
                physicalEvents[attachment] = {};
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
            U32 layers = ~0u;

            auto& renderPassInfo = physicalPass.renderPassInfo;
            U32 numColorAttachments = (U32)physicalPass.physicalColorAttachments.size();
            for (U32 i = 0; i < numColorAttachments; i++)
            {
                auto& att = renderPassInfo.colorAttachments[i];
                att = physicalAttachments[physicalPass.physicalColorAttachments[i]];
                if (att->GetImage()->GetCreateInfo().domain == GPU::ImageDomain::Physical)
                    layers = std::min(layers, att->GetImage()->GetCreateInfo().layers);
            }

            if (physicalPass.physicalDepthStencilAttachment != RenderResource::Unused)
            {
                auto& ds = renderPassInfo.depthStencil;
                ds = physicalAttachments[physicalPass.physicalDepthStencilAttachment];
                if (ds->GetImage()->GetCreateInfo().domain == GPU::ImageDomain::Physical)
                    layers = std::min(layers, ds->GetImage()->GetCreateInfo().layers);
            }
            else
            {
                renderPassInfo.depthStencil = nullptr;
            }      
        }
    }

    static bool NeedInvalidate(const Barrier& barrier, const RenderGraphEvent& ent)
    {
        bool needInvalidate = false;
        ForEachBit(barrier.stages, [&](U32 bit) {
            if (barrier.access & ~ent.invalidatedInStages[bit])
                needInvalidate = true;
        });
        return needInvalidate;
    }

    void RenderGraphImpl::HandleInvalidateBarrier(const Barrier& barrier, GPUPassSubmissionState& state, bool isGraphicsQueue)
    {
        auto& ent = physicalEvents[barrier.resIndex];

        bool needPipelineBarrier = false;
        bool needSempahore = false;
        bool layoutChange = false;

        GPU::SemaphorePtr waitSemaphore = isGraphicsQueue ? ent.waitGraphicsSemaphore : ent.waitComputeSemaphore;

        auto& physicalRes = physicalDimensions[barrier.resIndex];
        if (physicalRes.IsBuffer())
        {
            bool needSync = (ent.flushAccess != 0) || NeedInvalidate(barrier, ent);
            if (needSync)
            {
                // Wait for a pipline barrier
                needPipelineBarrier = ent.pieplineBarrierSrcStages != 0;
                needSempahore = bool(waitSemaphore);
            }

            if (needPipelineBarrier)
            {
                ASSERT(physicalBuffers[barrier.resIndex]);

                VkBufferMemoryBarrier b = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
                b.srcAccessMask = ent.flushAccess;
                b.dstAccessMask = barrier.access;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.buffer = physicalBuffers[barrier.resIndex]->GetBuffer();
                b.offset = 0;
                b.size = VK_WHOLE_SIZE;
                state.bufferBarriers.push_back(b);
            }
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
            bool needSync = layoutChange || (ent.flushAccess != 0) || NeedInvalidate(barrier, ent);
            if (needSync)
            {
                if (ent.pieplineBarrierSrcStages)
                {
                    // Wait for a pipline barrier
                    state.imageBarriers.push_back(b);
                    needPipelineBarrier = true;
                }
                else if (waitSemaphore)
                {
                    // Wait for a semaphore
                    if (layoutChange)
                    {
                        // When the semaphore was signalled, caches were flushed, so we don't need to do that again.
                        b.srcAccessMask = 0;
                        state.handoverBarriers.push_back(b);
                        state.handoverStages |= barrier.stages;
                    }
                    needSempahore = true;
                }
                else
                {
                    // Is the first time to use the resource, vkCmdPipelineBarrier from TOP_OF_PIPE_BIT
                    ASSERT(b.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
                    state.immediateImageBarriers.push_back(b);
                    state.immediateDstStages |= barrier.stages;
                }
            }
        }

        // Invalidate caches if and pending writes or layout changes
        if (ent.flushAccess != 0 || layoutChange)
        {
            for (auto& v : ent.invalidatedInStages)
                v = 0;
        }
        ent.flushAccess = 0;

        if (needPipelineBarrier)
        {
            ASSERT(ent.pieplineBarrierSrcStages);

            state.preSrcStages |= ent.pieplineBarrierSrcStages;
            state.preDstStages |= barrier.stages;

            // Mark appropriate caches
            ForEachBit(barrier.stages, [&](U32 bit) {
                ent.invalidatedInStages[bit] |= barrier.access;
            });
        }
        else if (needSempahore)
        {
            ASSERT(waitSemaphore);

            state.waitSemaphores.push_back(waitSemaphore);
            state.waitSemaphoreStages.push_back(barrier.stages);

            // Mark appropriate caches
            ForEachBit(barrier.stages, [&](U32 bit) {
                if (layoutChange)
                    ent.invalidatedInStages[bit] |= barrier.access;
                else
                    ent.invalidatedInStages[bit] = 0;
            });
        }
    }
    
    void RenderGraphImpl::HandleSignal(const PhysicalPass& physicalPass, GPUPassSubmissionState& state)
    {
        for (auto& barrier : physicalPass.flush)
        {
            auto& physicalDim = physicalDimensions[barrier.resIndex];
            if (physicalDim.UseSemaphore())
                state.needSubmissionSemaphore = true;
            else
                state.postPipelineBarrierStages |= barrier.stages;
        }

        if (state.needSubmissionSemaphore)
        {
            state.graphicsSemaphore = device->RequestEmptySemaphore();
            state.computeSemaphore = device->RequestEmptySemaphore();
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

        // Mark if there are pending writes from this pass
        ent.flushAccess = barrier.access;

        if (physicalRes.UseSemaphore())
        {
            ent.waitGraphicsSemaphore = state.graphicsSemaphore;
            ent.waitComputeSemaphore = state.computeSemaphore;
            ent.pieplineBarrierSrcStages = 0;
        }
        else
        {
            ent.pieplineBarrierSrcStages = state.postPipelineBarrierStages;
        }
    }

    void RenderGraphImpl::DoGraphicsCommands(GPU::CommandList& cmd, const PhysicalPass& physicalPass, GPUPassSubmissionState* state)
    {
        cmd.BeginEvent(state->name);

        // Clear colors
        for (auto& colorClear : physicalPass.colorClearRequests)
            colorClear.pass->GetClearColor(colorClear.index, colorClear.target);

        // Clear depth stencil
        if (physicalPass.depthClearRequest.target != nullptr)
            physicalPass.depthClearRequest.pass->GetClearDepthStencil(physicalPass.depthClearRequest.target);

        // Begin render pass
        cmd.BeginRenderPass(physicalPass.renderPassInfo);

        // Handle subpasses
        for (U32 i = 0; i < physicalPass.passes.size(); i++)
        {
            auto& passIndex = physicalPass.passes[i];
            auto& pass = *renderPasses[passIndex];
  
            // Profiler begin
            if (physicalPass.passes.size() > 1)
                Profiler::BeginBlock(pass.GetName().c_str());

            cmd.BeginEvent(pass.GetName().c_str());
            pass.BuildRenderPass(cmd);
            cmd.EndEvent();

            if (i < (physicalPass.passes.size() - 1))
                cmd.NextSubpass(VK_SUBPASS_CONTENTS_INLINE);

            // Profiler end
            if (physicalPass.passes.size() > 1)
                Profiler::EndBlock();
        }

        // End render pass
        cmd.EndRenderPass();

        cmd.EndEvent();
    }

    void RenderGraphImpl::DoComputeCommands(GPU::CommandList& cmd, const PhysicalPass& physicalPass, GPUPassSubmissionState* state)
    {
        ASSERT(physicalPass.passes.size() == 1);
        auto& pass = *renderPasses[physicalPass.passes[0]];
        cmd.BeginEvent(pass.GetName().c_str());
        pass.BuildRenderPass(cmd);
        cmd.EndEvent();
    }

    void RenderGraphImpl::TransferOwnership(const PhysicalPass& physicalPass)
    {
        for (auto& transfer : physicalPass.aliasTransfers)
        {
            // Wait on the event before we transfer to alias
            auto& ent = physicalEvents[transfer.second];
            ent = physicalEvents[transfer.first];
            for (auto& v : ent.invalidatedInStages)
                v = 0;

            ASSERT(ent.flushAccess == 0);
            ent.flushAccess = 0;
            ent.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }

    void RenderGraphImpl::HandleTimelineGPU(GPU::DeviceVulkan& device, const PhysicalPass& physicalPass, GPUPassSubmissionState* state, U8 index)
    {
        ASSERT(state->renderingDependency.counter == 0);
        Jobsystem::Run(state, [this, &device, &physicalPass](void* data)->void {
            GPUPassSubmissionState* state = (GPUPassSubmissionState*)data;
            if (state == nullptr)
                return;

            PROFILE_BLOCK(state->name);
            GPU::CommandListPtr cmd = device.RequestCommandList(state->queueType);
            state->cmd = cmd;

            state->EmitPrePassBarriers();

            if (state->isGraphics)
                DoGraphicsCommands(*cmd, physicalPass, state);
            else
                DoComputeCommands(*cmd, physicalPass, state);

            // We end this cmd on a same thread we requested it on
            cmd->EndCommandBufferForThread();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
            Logger::Print("Pass %s execute", state->name);
#endif

        // TODO: need to limit worker index?
        }, & state->renderingDependency, index);
    }

    void RenderGraphImpl::EnqueueRenderPass(PhysicalPass& physicalPass, GPUPassSubmissionState& state)
    {
        // Check render pass requires work
        bool requiresWork = false;
        for (auto& pass : physicalPass.passes)
        {
            if (renderPasses[pass]->NeedRenderPass())
            {
                requiresWork = true;
                break;
            }
        }
        if (!requiresWork)
        {
            TransferOwnership(physicalPass);
            return;
        }

        // Get queue type
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
        {
            bool physicalGraphics = device->GetPhysicalQueueType(state.queueType) == GPU::QUEUE_INDEX_GRAPHICS;
            HandleInvalidateBarrier(barrier, state, physicalGraphics);
        }

        HandleSignal(physicalPass, state);

        // Handle flush barriers
        for (auto& barrier : physicalPass.flush)
            HandleFlushBarrier(barrier, state);

        // Handle alias transfer of physical pass
        TransferOwnership(physicalPass);

        state.subpassContents.resize(physicalPass.passes.size());
        for (auto& c : state.subpassContents)
            c = VK_SUBPASS_CONTENTS_INLINE;

        // Prepare render passes
        for (auto& passIndex : physicalPass.passes)
        {
            RenderPass& subpass = *renderPasses[passIndex];
            subpass.EnqueuePrepareRenderPass();
        }

        state.active = true;
    }

    void RenderGraphImpl::SwapchainLayoutTransitionPass()
    {
        U32 resIndex = nameToResourceIndex[backbufferSource];
        U32 index = resources[resIndex]->GetPhysicalIndex();
        if (physicalEvents[index].imageLayout == swapchainLayout)
            return;

        bool isGraphics = physicalDimensions[resIndex].queues & (U32)RenderGraphQueueFlag::Graphics;
        auto& waitSemaphore = isGraphics ? physicalEvents[index].waitGraphicsSemaphore : physicalEvents[index].waitComputeSemaphore;

        GPU::Image* image = physicalAttachments[index]->GetImage();

        auto cmd = device->RequestCommandList(GPU::QueueType::QUEUE_TYPE_GRAPHICS);
        cmd->BeginEvent("RenderGraphLayoutTransition");
        if (physicalEvents[index].pieplineBarrierSrcStages != 0)
        {
            VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.image = image->GetImage();
            barrier.oldLayout = physicalEvents[index].imageLayout;
            barrier.newLayout = swapchainLayout;
            barrier.srcAccessMask = physicalEvents[index].flushAccess;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.levelCount = image->GetCreateInfo().levels;
            barrier.subresourceRange.layerCount = image->GetCreateInfo().layers;
            barrier.subresourceRange.aspectMask = GPU::formatToAspectMask(image->GetCreateInfo().format);

            cmd->Barrier(
                physicalEvents[index].pieplineBarrierSrcStages,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, nullptr,
                1, &barrier
            );

            physicalEvents[index].imageLayout = swapchainLayout;
        }
        else
        {
            if (waitSemaphore && waitSemaphore->GetSemaphore() != VK_NULL_HANDLE && !waitSemaphore->IsPendingWait())
                device->AddWaitSemaphore(GPU::QueueType::QUEUE_TYPE_GRAPHICS, waitSemaphore, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, true);

            cmd->ImageBarrier(*image,
                physicalEvents[index].imageLayout, swapchainLayout,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);

            physicalEvents[index].imageLayout = swapchainLayout;
        }

        physicalEvents[index].flushAccess = 0;
        for (auto& e : physicalEvents[index].invalidatedInStages)
            e = 0;
        physicalEvents[index].invalidatedInStages[TrailingZeroes(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)] = VK_ACCESS_SHADER_READ_BIT;
        cmd->EndEvent();

#if 0
        GPU::SemaphorePtr sem;
        device->Submit(cmd, nullptr, 1, &sem);
        device->AddWaitSemaphore(GPU::QueueIndices::QUEUE_INDEX_GRAPHICS, sem, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, true);
#else
        device->Submit(cmd);
#endif
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
            EnqueueRenderPass(physicalPass, state);
        }

        for (U32 i = 0; i < submissionStates.size(); i++)
        {
            auto& state = submissionStates[i];
            if (state.active)
                HandleTimelineGPU(device, physicalPasses[i], &state, (U8)i);
        }

        // Sequential submit all states
        ASSERT(submitHandle.counter == 0);
        Jobsystem::Run(nullptr, [this](void* data)->void {
            PROFILE_BLOCK("Submit states");
            for (auto& state : submissionStates)
            {
                if (state.active == false)
                    continue;

                Jobsystem::JobHandle* renderingDependency = nullptr;
                if (state.renderingDependency)
                    renderingDependency = &state.renderingDependency;

                // Wait GPU behavior
                Jobsystem::Wait(renderingDependency);

                // Submit state
                state.Submit();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
                Logger::Print("Pass %s submit", state->name);
#endif
            }
        }, &submitHandle);

        // Flush swapchain
        if (swapchainPhysicalIndex == RenderResource::Unused)
        {
            Jobsystem::Run(nullptr, [&device, this](void* data)->void {
                Jobsystem::Wait(&submitHandle);

                if (swapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED)
                    SwapchainLayoutTransitionPass();

                device.FlushFrames();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
                Logger::Print("FlushFrames");
#endif
                }, &jobHandle);
        }
        else
        {
            Jobsystem::Run(nullptr, [&device, this](void* data)->void {
                Jobsystem::Wait(&submitHandle);
                device.FlushFrames();

#if RENDER_GRAPH_LOGGING_LEVEL >= 1
                Logger::Print("FlushFrames");
#endif
                }, &jobHandle);
        }
      
        device.FlushFrames();
    }

    void RenderGraphImpl::Reset()
    {
        swapchainEnable = true;
        nameToRenderPassIndex.clear();
        renderPasses.clear();
        resources.clear();
        nameToResourceIndex.clear();
        physicalPasses.clear();
        physicalDimensions.clear();
        physicalAttachments.clear();
        physicalBuffers.clear();
        physicalImages.clear();
        physicalEvents.clear();
    }

    void RenderGraphImpl::Log()
    {
        Logger::Info("---------------------------------------------------------------------------");
        Logger::Info("RenderGraph");
        Logger::Info("---------------------------------------------------------------------------");
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

        Logger::Info("---------------------------------------------------------------------------");
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
        // Barriers
        if (!immediateImageBarriers.empty() || 
            !handoverBarriers.empty() ||
            !imageBarriers.empty() ||
            !bufferBarriers.empty())
        {
            std::vector<VkImageMemoryBarrier> combinedBarriers;
            combinedBarriers.reserve(
                handoverBarriers.size() + 
                immediateImageBarriers.size() +
                imageBarriers.size()
            );

            combinedBarriers.insert(combinedBarriers.end(), handoverBarriers.begin(), handoverBarriers.end());
            combinedBarriers.insert(combinedBarriers.end(), immediateImageBarriers.begin(), immediateImageBarriers.end());
            combinedBarriers.insert(combinedBarriers.end(), imageBarriers.begin(), imageBarriers.end());

            VkPipelineStageFlags src = handoverStages | preSrcStages;
            VkPipelineStageFlags dst = handoverStages | immediateDstStages | preDstStages;
            if (src == 0)
                src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            cmd->Barrier(src, dst, 
                bufferBarriers.size(), bufferBarriers.empty() ? nullptr : bufferBarriers.data(),
                combinedBarriers.size(), combinedBarriers.empty() ? nullptr : combinedBarriers.data());
        }
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

    bool RenderPassJob::IsConditional()const
    {
        return false;
    }

    bool RenderPassJob::GetClearDepthStencil(VkClearDepthStencilValue* value) const
    {
        if (value)
            *value = { 1.0f, 0u };
        return true;
    }

    bool RenderPassJob::GetClearColor(unsigned attachment, VkClearColorValue* value) const
    {
        if (value)
            *value = {};
        return true;
    }

    void RenderPassJob::Setup(GPU::DeviceVulkan& device)
    {
    }

    void RenderPassJob::SetupDependencies(RenderGraph& graph)
    {
    }

    void RenderPassJob::EnqueuePrepare(RenderGraph& graph)
    {
    }

    void RenderPassJob::BuildRenderPass(GPU::CommandList& cmd)
    {
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

    RenderTextureResource& RenderPass::WriteColor(const char* name, const AttachmentInfo& info, const char* input)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        res.SetImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        res.SetAttachmentInfo(info);
        outputColors.push_back(&res);

        if (input != nullptr)
        {
            auto& inputRes = graph.GetOrCreateTexture(input);
            inputRes.PassRead(index);
            inputRes.SetImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
            inputColors.push_back(&res);
        }
        else
        {
            inputColors.push_back(nullptr);
        }
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

    RenderTextureResource& RenderPass::WriteStorageTexture(const char* name, const AttachmentInfo& info, const char* input)
    {
        auto& res = graph.GetOrCreateTexture(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        res.SetAttachmentInfo(info);
        res.SetImageUsage(VK_IMAGE_USAGE_STORAGE_BIT);
        outputStorageTextures.push_back(&res);
        
        if (input != nullptr && input[0] != '\0')
        {
            auto& inputRes = graph.GetOrCreateTexture(input);
            inputRes.PassRead(index);
            inputRes.SetImageUsage(VK_IMAGE_USAGE_STORAGE_BIT);
            inputStorageTextures.push_back(&res);
        }
        else
        {
            inputStorageTextures.push_back(nullptr);
        }

        return res;
    }

    RenderBufferResource& RenderPass::ReadStorageBuffer(const char* name)
    {
        auto& res = graph.GetOrCreateBuffer(name);
        res.AddUsedQueue(queue);
        res.PassRead(index);
        inputStorageBuffers.push_back(&res);
        return res;
    }

    RenderBufferResource& RenderPass::WriteStorageBuffer(const char* name, const BufferInfo& info, const char* input)
    {
        auto& res = graph.GetOrCreateBuffer(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);
        outputStorageBuffers.push_back(&res);
        return res;
    }

    void RenderPass::AddProxyOutput(const char* name, VkPipelineStageFlags stages)
    {
        auto& res = graph.GetOrCreateProxy(name);
        res.AddUsedQueue(queue);
        res.PassWrite(index);

        AccessedProxyResource proxy;
        proxy.proxy = &res;
        proxy.layout = VK_IMAGE_LAYOUT_GENERAL;
        proxy.stages = stages;
        proxyOutputs.push_back(proxy);
    }

    void RenderPass::AddProxyInput(const char* name, VkPipelineStageFlags stages)
    {
        auto& res = graph.GetOrCreateProxy(name);
        res.AddUsedQueue(queue);
        res.PassRead(index);

        AccessedProxyResource proxy;
        proxy.proxy = &res;
        proxy.layout = VK_IMAGE_LAYOUT_GENERAL;
        proxy.stages = stages;
        proxyInputs.push_back(proxy);
    }

    void RenderPass::AddFakeResourceWriteAlias(const char* from, const char* to)
    {
        auto& fromRes = graph.GetOrCreateTexture(from);
        auto& toRes = graph.GetOrCreateTexture(to);
        toRes = fromRes;
        toRes.SetName(to);
        toRes.GetReadPasses().clear();
        toRes.GetWrittenPasses().clear();
        toRes.PassWrite(index);
        fakeResourceAlias.emplace_back(&fromRes, &toRes);
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
        impl->Reset();
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

        U32 index = (U32)impl->renderPasses.size();
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

    const char* RenderGraph::GetBackBufferSource()
    {
        return impl->backbufferSource.c_str();
    }

    void RenderGraph::DisableSwapchain()
    {
        impl->swapchainEnable = false;
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

        Logger::Info("RenderGraph baking... backbuffer:%s size:%dx%d", backbuffer.name.c_str(), impl->swapchainDimensions.width, impl->swapchainDimensions.height);

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
        if (!impl->swapchainEnable || !canAliasBackbuffer || backbufferDim != impl->swapchainDimensions)
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

        impl->isBaked = true;
        Logger::Info("RenderGraph finishd baking.");
    }

    void RenderGraph::SetupAttachments(GPU::DeviceVulkan& device, GPU::ImageView* swapchain, VkImageLayout finalLayout)
    {
        impl->SetupAttachments(device, swapchain, finalLayout);
    }
    
    void RenderGraph::Render(GPU::DeviceVulkan& device, Jobsystem::JobHandle& jobHandle)
    {
        impl->Render(device, jobHandle);
    }

    bool RenderGraph::IsBaked()
    {
        bool ret = impl->isBaked;
        impl->isBaked = false;
        return ret;
    }

    void RenderGraph::Log()
    {
        impl->Log();
    }

    void RenderGraph::ExportGraphviz()
    {
        impl->DebugExportGraphviz();
    }

    void RenderGraph::GetDebugRenderPassInfos(std::vector<DebugRenderPassInfo>& infos)
    {
        for (auto& physicalPass : impl->physicalPasses)
        {
            for (auto& subpass : physicalPass.passes)
            {
                RenderPass& pass = *impl->renderPasses[subpass];

                DebugRenderPassInfo info = {};
                info.name = pass.name;

                // Input
                //for (auto& tex : pass.GetInputColors())
                //{
                //    if (tex != nullptr)
                //        info.reads.push_back(tex->GetName());
                //}     

                for (auto& tex : pass.GetInputTextures())
                    info.reads.push_back(tex.texture->GetName());

                for (auto& tex : pass.GetInputStorageTextures())
                {
                    if (tex != nullptr)
                        info.reads.push_back(tex->GetName());
                }

                for (auto& buffer : pass.GetInputStorageBuffers())
                    info.reads.push_back(buffer->GetName());

                if (pass.GetInputDepthStencil())
                    info.reads.push_back(pass.GetInputDepthStencil()->GetName());

                // Output
                for (auto& tex :pass.GetOutputColors())
                    info.writes.push_back(tex->GetName());

                for (auto& buffer : pass.GetOutputStorageBuffers())
                    info.writes.push_back(buffer->GetName());

                for (auto& tex : pass.GetOutputStorageTextures())
                    info.writes.push_back(tex->GetName());

                if (pass.GetOutputDepthStencil())
                    info.writes.push_back(pass.GetOutputDepthStencil()->GetName());

                infos.push_back(info);
            }  
        }
    }

    RenderTextureResource& RenderGraph::GetOrCreateTexture(const char* name)
    {
        auto it = impl->nameToResourceIndex.find(name);
        if (it != impl->nameToResourceIndex.end())
            return *static_cast<RenderTextureResource*>(impl->resources[it->second].Get());

        U32 index = (U32)impl->resources.size();
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

        U32 index = (U32)impl->resources.size();
        impl->resources.emplace_back(CJING_NEW(RenderBufferResource)(index));
        auto& res = *impl->resources.back();
        res.name = name;
        impl->nameToResourceIndex[name] = index;
        return *static_cast<RenderBufferResource*>(&res);
    }

    RenderResource& RenderGraph::GetOrCreateProxy(const char* name)
    {
        auto it = impl->nameToResourceIndex.find(name);
        if (it != impl->nameToResourceIndex.end())
            return *static_cast<RenderResource*>(impl->resources[it->second].Get());

        U32 index = (U32)impl->resources.size();
        impl->resources.emplace_back(CJING_NEW(RenderResource)(index, RenderGraphResourceType::Proxy));
        auto& res = *impl->resources.back();
        res.name = name;
        impl->nameToResourceIndex[name] = index;
        return *static_cast<RenderResource*>(&res);
    }

    GPU::ImageView& RenderGraph::GetPhysicalTexture(const RenderTextureResource& res)
    {
        ASSERT(res.GetPhysicalIndex() != RenderResource::Unused);
        return *impl->physicalAttachments[res.GetPhysicalIndex()];
    }

    GPU::ImageView* RenderGraph::TryGetPhysicalTexture(RenderTextureResource* res)
    {
        if (res == nullptr || res->GetPhysicalIndex() == RenderResource::Unused)
            return nullptr;

        if (impl->physicalAttachments.empty())
            return nullptr;

        return impl->physicalAttachments[res->GetPhysicalIndex()];
    }

    GPU::Buffer& RenderGraph::GetPhysicalBuffer(const RenderBufferResource& res)
    {
        ASSERT(res.GetPhysicalIndex() != RenderResource::Unused);
        return *impl->physicalBuffers[res.GetPhysicalIndex()];
    }

    GPU::Buffer* RenderGraph::TryGetPhysicalBuffer(RenderBufferResource* res)
    {
        if (res == nullptr || res->GetPhysicalIndex() == RenderResource::Unused)
            return nullptr;
        return impl->physicalBuffers[res->GetPhysicalIndex()].get();
    }

    void RenderGraph::SetBackbufferDimension(const ResourceDimensions& dim)
    {
        impl->swapchainDimensions = dim;
    }
}
