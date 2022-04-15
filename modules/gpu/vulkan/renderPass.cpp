#include "renderPass.h"
#include "vulkan/device.h"

namespace VulkanTest
{
namespace GPU
{

namespace {
	VkAttachmentLoadOp CheckLoadOp(const RenderPassInfo& info, uint32_t i)
	{
		if ((info.clearAttachments & (1u << i)) != 0)
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		else if ((info.loadAttachments & (1u << i)) != 0)
			return  VK_ATTACHMENT_LOAD_OP_LOAD;
		else
			return  VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	VkAttachmentStoreOp CheckStoreOp(const RenderPassInfo& info, uint32_t i)
	{
		if ((info.storeAttachments & (1u << i)) != 0)
			return VK_ATTACHMENT_STORE_OP_STORE;
		else
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}

	VkAttachmentReference* FindSubpassColor(std::vector<VkSubpassDescription>& subpasses, unsigned subpass, unsigned attachment)
	{
		if (subpasses[subpass].pColorAttachments == nullptr)
			return nullptr;

		auto* colors = subpasses[subpass].pColorAttachments;
		for (unsigned i = 0; i < subpasses[subpass].colorAttachmentCount; i++)
			if (colors[i].attachment == attachment)
				return const_cast<VkAttachmentReference*>(&colors[i]);
		return nullptr;
	};


	VkAttachmentReference* FindSubpassResolve(std::vector<VkSubpassDescription>& subpasses, unsigned subpass, unsigned attachment)
	{
		if (subpasses[subpass].pResolveAttachments == nullptr)
			return nullptr;

		auto* resolves = subpasses[subpass].pResolveAttachments;
		for (unsigned i = 0; i < subpasses[subpass].colorAttachmentCount; i++)
			if (resolves[i].attachment == attachment)
				return const_cast<VkAttachmentReference*>(&resolves[i]);
		return nullptr;
	};


	VkAttachmentReference* FindSubpassInput(std::vector<VkSubpassDescription>& subpasses, unsigned subpass, unsigned attachment)
	{
		if (subpasses[subpass].pInputAttachments == nullptr)
			return nullptr;

		auto* inputs = subpasses[subpass].pInputAttachments;
		for (unsigned i = 0; i < subpasses[subpass].inputAttachmentCount; i++)
			if (inputs[i].attachment == attachment)
				return const_cast<VkAttachmentReference*>(&inputs[i]);
		return nullptr;
	};

	VkAttachmentReference* FindSubpassDepthStencil(std::vector<VkSubpassDescription>& subpasses, unsigned subpass, unsigned attachment)
	{
		if (subpasses[subpass].pDepthStencilAttachment == nullptr)
			return nullptr;

		if (subpasses[subpass].pDepthStencilAttachment->attachment == attachment)
			return const_cast<VkAttachmentReference*>(subpasses[subpass].pDepthStencilAttachment);
		else
			return nullptr;
	};
}

void RenderPass::SetupSubPasses(const VkRenderPassCreateInfo& info)
{
	for (U32 i = 0; i < info.subpassCount; i++)
	{
		auto& subpass = info.pSubpasses[i];

		auto& subpassInfo = initedSubpassInfos.emplace_back();
		subpassInfo.numColorAttachments = subpass.colorAttachmentCount;
		subpassInfo.numInputAttachments = subpass.inputAttachmentCount;
		subpassInfo.depthStencilAttachment = *subpass.pDepthStencilAttachment;
		memcpy(subpassInfo.colorAttachments, subpass.pColorAttachments, subpass.colorAttachmentCount * sizeof(*subpass.pColorAttachments));
		memcpy(subpassInfo.inputAttachments, subpass.pInputAttachments, subpass.inputAttachmentCount * sizeof(*subpass.pInputAttachments));
	
		U32 samples = 0;
		for (auto& colorAtt : subpassInfo.colorAttachments)
		{
			if (colorAtt.attachment == VK_ATTACHMENT_UNUSED)
				continue;

			samples = info.pAttachments[colorAtt.attachment].samples;
		}

		if (subpassInfo.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED)
		{
			samples = info.pAttachments[subpassInfo.depthStencilAttachment.attachment].samples;
		}

		assert(samples > 0);
		subpassInfo.samples = samples;
	}
}

RenderPass::RenderPass(DeviceVulkan& device_, const RenderPassInfo& info) :
	device(device_)
{
	// Fill color attachments
	for (int i = 0; i < VULKAN_NUM_ATTACHMENTS; i++)
		colorAttachments[i] = VK_FORMAT_UNDEFINED;

	//  Setup default subpass info if we don't have it
	uint32_t numSubpasses = info.numSubPasses;
	const RenderPassInfo::SubPass* subpassInfos = info.subPasses;
	RenderPassInfo::SubPass defaultSupassInfo;
	if (!subpassInfos)
	{
		defaultSupassInfo.numColorAattachments = info.numColorAttachments;
		defaultSupassInfo.depthStencilMode = DepthStencilMode::ReadWrite;
		for (U32 i = 0; i < info.numColorAttachments; i++)
			defaultSupassInfo.colorAttachments[i] = i;

		numSubpasses = 1;
		subpassInfos = &defaultSupassInfo;
	}

	uint32_t numAttachments = info.numColorAttachments + (info.depthStencil != nullptr ? 1 : 0);
	U32 implicitTransitions = 0;
	U32 implicitBottomOfPipe = 0;

	// Color attachments
	VkAttachmentDescription attachments[VULKAN_NUM_ATTACHMENTS + 1] = {};
	for (uint32_t i = 0; i < info.numColorAttachments; i++)
	{
		colorAttachments[i] = info.colorAttachments[i]->GetFormat();

		auto& attachment = attachments[i];
		attachment.flags = 0;
		attachment.format = colorAttachments[i];
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = CheckLoadOp(info, i);
		attachment.storeOp = CheckStoreOp(info, i);
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		const Image* image = info.colorAttachments[i]->GetImage();
		if (image->GetCreateInfo().domain == ImageDomain::Transient)
		{
		}
		else if (image->IsSwapchainImage())
		{
			// Keep initial layout
			if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
				attachment.initialLayout = image->GetSwapchainLayout();

			attachment.finalLayout = image->GetSwapchainLayout();

			// 如果我们从PRESENT_SRC_KHR开始Transition，会存在一个external subpass dependency发生在BOTTOM_OF_PIPE.
			if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
				implicitBottomOfPipe |= 1u << i;

			implicitTransitions |= 1u << i;
		}
		else
		{
			attachment.initialLayout = info.colorAttachments[i]->GetImage()->GetLayoutType() == ImageLayoutType::Optimal ? 
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_GENERAL;
		}
	}
	
	// Depth stencil
	if (info.depthStencil)
	{
		VkImageLayout dsLayout = VK_IMAGE_LAYOUT_GENERAL;
		if (info.depthStencil->GetImage()->GetLayoutType() == ImageLayoutType::Optimal)
		{
			bool dsReadOnly = (info.opFlags & RENDER_PASS_OP_DEPTH_STENCIL_READ_ONLY_BIT) != 0;
			dsLayout = dsReadOnly ?
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		depthStencil = info.depthStencil->GetFormat();

		auto& attachment = attachments[info.numColorAttachments + 1];
		attachment.format = depthStencil;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = dsLayout;
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	Util::StackAllocator<VkAttachmentReference, 512> attachmentRefAllocator;
	Util::StackAllocator<U32, 1024> preserveAllocator;

	std::vector< VkSubpassDescription> subpasses(numSubpasses);
	for (uint32_t i = 0; i < numSubpasses; i++)
	{
		const RenderPassInfo::SubPass& subpassInfo = subpassInfos[i];
		VkAttachmentReference* colors = attachmentRefAllocator.Allocate(subpassInfo.numColorAattachments);
		VkAttachmentReference* inputs = attachmentRefAllocator.Allocate(subpassInfo.numInputAttachments);
		VkAttachmentReference* resolves = attachmentRefAllocator.Allocate(subpassInfo.numResolveAttachments);
		VkAttachmentReference* depth = attachmentRefAllocator.Allocate();

		VkSubpassDescription& subpass = subpasses[i];
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.inputAttachmentCount = subpassInfo.numInputAttachments;
		subpass.pInputAttachments = inputs;
		subpass.colorAttachmentCount = subpassInfo.numColorAattachments;
		subpass.pColorAttachments = colors;
		subpass.pDepthStencilAttachment = depth;

		// colors
		for (U32 j = 0; j < subpassInfo.numColorAattachments; j++)
		{
			assert(subpassInfo.colorAttachments[j] == VK_ATTACHMENT_UNUSED || subpassInfo.colorAttachments[j] < numAttachments);
			colors[j].attachment = subpassInfo.colorAttachments[j];
			colors[j].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
		
		// inputs
		for (U32 j = 0; j < subpassInfo.numInputAttachments; j++)
		{
			assert(subpassInfo.inputAttachments[j] == VK_ATTACHMENT_UNUSED || subpassInfo.inputAttachments[j] < numAttachments);
			colors[j].attachment = subpassInfo.inputAttachments[j];
			colors[j].layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		// resolve
		if (subpassInfo.numResolveAttachments > 0)
		{
			assert(subpassInfo.numColorAattachments == subpassInfo.numResolveAttachments);
			subpass.pResolveAttachments = resolves;

			for (U32 j = 0; j < subpassInfo.numColorAattachments; j++)
			{
				assert(subpassInfo.resolveAttachments[j] == VK_ATTACHMENT_UNUSED || subpassInfo.resolveAttachments[j] < numAttachments);
				resolves[j].attachment = subpassInfo.resolveAttachments[j];
				resolves[j].layout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
		}

		// depth
		if (info.depthStencil != nullptr && subpassInfo.depthStencilMode != DepthStencilMode::None)
		{
			depth->attachment = info.numColorAttachments;
			depth->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
		else
		{
			depth->attachment = VK_ATTACHMENT_UNUSED;
			depth->layout = VK_IMAGE_LAYOUT_UNDEFINED;
		}
	}

	U32 lastSubpassForAttachment[VULKAN_NUM_ATTACHMENTS + 1] = {};
	// 找到未在Subpass使用且需要保留的Attachment
	uint32_t preserveMasks[VULKAN_NUM_ATTACHMENTS + 1] = {};

	// subpass相关attchament操作的mask,用于构建subpass之间的dependency stageMask
	U32 colorAttachmentReadWriteSubpassMask = 0;
	U32 inputAttachmentReadSubpassMask = 0;
	U32 depthAttachmentWriteSubpassMask = 0;
	U32 depthAttachmentReadSubpassMask = 0;

	// 处理每一个Attachemnt及其相关的Subpasses
	// 设置Attachment在对应subpass阶段所使用的layout(AttachmentReference)
	// 同时获取相关外部依赖(color,depth, input)

	U32 externalColorDependencies = 0;
	U32 externalDepthDependencies = 0;
	U32 externalInputDependencies = 0;
	U32 externalSubpassBottomOfPipeDependencies = 0;
	for (U32 attachment = 0; attachment < numAttachments; attachment++)
	{
		U32 attachmentBit = 1u << attachment;
		bool used = false;

		VkImageLayout currentLayout = attachments[attachment].initialLayout;
		for (U32 subpass = 0; subpass < numSubpasses; subpass++)
		{
			auto& subpassInfo = subpassInfos[subpass];
			auto* color = FindSubpassColor(subpasses, subpass, attachment);
			auto* resolve = FindSubpassResolve(subpasses, subpass, attachment);
			auto* input = FindSubpassInput(subpasses, subpass, attachment);
			auto* depth = FindSubpassDepthStencil(subpasses, subpass, attachment);

			// Sanity check
			if (color) assert(!depth);
			if (depth) assert(!color && !resolve);
			if (resolve) assert(!color && !depth);
			if (!color && !resolve && !input && !depth)
			{
				if (used)
					preserveMasks[attachment] |= 1u << subpass;
				continue;
			}
			
			// attachment属于TransientImage或者SwapchainImage
			if (!used && (implicitTransitions & attachmentBit))
			{
				if (color != nullptr)
					externalColorDependencies |= 1u << subpass;
				if (input != nullptr)
					externalInputDependencies |= 1u << subpass;
				if (depth != nullptr)
					externalDepthDependencies |= 1u << subpass;
			}

			// 设置参与implicitBottomOfPipe的subpass
			if (!used && (implicitBottomOfPipe & attachmentBit))
				externalSubpassBottomOfPipeDependencies |= 1u << subpass;

			if (color && input)
			{
				// image support input/output
				currentLayout = VK_IMAGE_LAYOUT_GENERAL;
				color->layout = currentLayout;
				input->layout = currentLayout;

				if (!used && attachments[attachment].initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
					attachments[attachment].initialLayout = currentLayout;

				// 如果是第一个转移layout的subpass,我们需要注入一个external subpass dependency
				if (!used && attachments[attachment].initialLayout != currentLayout)
				{
					externalColorDependencies |= 1u << subpass;
					externalInputDependencies |= 1u << subpass;
				}

				used = true;
				colorAttachmentReadWriteSubpassMask |= 1u << subpass;
				inputAttachmentReadSubpassMask |= 1u << subpass;
			}
			else if (color)
			{
				if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
					currentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				color->layout = currentLayout;

				// 如果是第一个转移layout的subpass,我们需要注入一个external subpass dependency
				if (!used && attachments[attachment].initialLayout != currentLayout)
					externalColorDependencies |= 1u << subpass;

				used = true;
				colorAttachmentReadWriteSubpassMask |= 1u << subpass;
			}
			else if (depth && input)
			{
				if (subpassInfo.depthStencilMode == DepthStencilMode::ReadWrite)
				{
					currentLayout = VK_IMAGE_LAYOUT_GENERAL;
					depthAttachmentWriteSubpassMask |= 1u << subpass;

					if (!used && attachments[attachment].initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
						attachments[attachment].initialLayout = currentLayout;
				}
				else
				{
					if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
						currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}

				// 如果是第一个转移layout的subpass,我们需要注入一个external subpass dependency
				if (!used && attachments[attachment].initialLayout != currentLayout)
				{
					externalDepthDependencies |= 1u << subpass;
					externalInputDependencies |= 1u << subpass;
				}

				used = true;
				inputAttachmentReadSubpassMask |= 1u << subpass;
				depthAttachmentReadSubpassMask |= 1u << subpass;
				depth->layout = currentLayout;
				input->layout = currentLayout;
			}
			else if (depth)
			{
				if (subpassInfo.depthStencilMode == DepthStencilMode::ReadWrite)
				{
					depthAttachmentWriteSubpassMask |= 1u << subpass;
					if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
						currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				}
				else
				{
					if (currentLayout != VK_IMAGE_LAYOUT_GENERAL)
						currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}

				// 如果是第一个转移layout的subpass,我们需要注入一个external subpass dependency
				if (!used && attachments[attachment].initialLayout != currentLayout)
					externalDepthDependencies |= 1u << subpass;

				used = true;
				depthAttachmentReadSubpassMask |= 1u << subpass;
				depth->layout = currentLayout;
			}
			else
			{
				assert(0);
			}

			lastSubpassForAttachment[attachment] = subpass;
		}
		assert(used);

		// 如果当前attachment未设置finalLayout，则使用经过pass后的currentLayout
		if (attachments[attachment].finalLayout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			ASSERT(currentLayout != VK_IMAGE_LAYOUT_UNDEFINED);
			attachments[attachment].finalLayout = currentLayout;
		}
	}

	// Add preserve attachments
	for (U32 attachment = 0; attachment < numAttachments; attachment++)
		preserveMasks[attachment] &= ((1u << lastSubpassForAttachment[attachment]) - 1);

	for (U32 subpass = 0; subpass < numSubpasses; subpass++)
	{
		auto& pass = subpasses[subpass];	
		U32 preserveCount = 0;
		for (U32 attachment = 0; attachment < numAttachments; attachment++)
		{
			if (preserveMasks[attachment] & (1u << subpass))
				preserveCount++;
		}

		U32* data = preserveAllocator.Allocate(preserveCount);
		for (U32 attachment = 0; attachment < numAttachments; attachment++)
			if (preserveMasks[attachment] & (1u << subpass))
				*data++ = attachment;

		pass.pPreserveAttachments = data;
		pass.preserveAttachmentCount = preserveCount;
	}

	// subpass dependencies
	std::vector<VkSubpassDependency> subpassDependencies;

	// add external subpass dependenices
	ForEachBit(externalColorDependencies | externalDepthDependencies| externalInputDependencies,
		[&](U32 subpass) {
			auto& dep = subpassDependencies.emplace_back();
			dep.srcSubpass = VK_SUBPASS_EXTERNAL;
			dep.dstSubpass = subpass;

			if (externalSubpassBottomOfPipeDependencies & (1u << subpass))
				dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

			if (externalColorDependencies & (1u << subpass))
			{
				dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dep.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			}

			if (externalDepthDependencies & (1u << subpass))
			{
				dep.srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				dep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			}

			if (externalInputDependencies & (1u << subpass))
			{
				dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				dep.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

				dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				dep.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			}
		});

	// Add dependencies between each subpass.
	for (U32 subpass = 1; subpass < numSubpasses; subpass++)
	{
		auto& dep = subpassDependencies.emplace_back();
		dep.srcSubpass = subpass - 1;
		dep.dstSubpass = subpass;
		dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// color write/read
		if (colorAttachmentReadWriteSubpassMask & (1u << (subpass - 1)))
		{
			dep.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		// depth stencil write
		if (depthAttachmentWriteSubpassMask & (1u << (subpass - 1)))
		{
			dep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dep.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}

		// color write/read
		if (colorAttachmentReadWriteSubpassMask & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		}
		// depth stencil read
		if (depthAttachmentReadSubpassMask & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		// input read
		if (inputAttachmentReadSubpassMask & (1u << subpass))
		{
			dep.dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dep.dstAccessMask |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		}
	}

	// Create render pass
	VkRenderPassCreateInfo rpInfo = {};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = numAttachments;
	rpInfo.pAttachments = attachments;
	rpInfo.subpassCount = numSubpasses;
	rpInfo.pSubpasses = subpasses.data();

	if (!subpassDependencies.empty())
	{
		rpInfo.dependencyCount = (U32)subpassDependencies.size();
		rpInfo.pDependencies = subpassDependencies.data();
	}

	// Setup subpasses
	SetupSubPasses(rpInfo);

	VkResult res = vkCreateRenderPass(device.device, &rpInfo, nullptr, &renderPass);
	assert(res == VK_SUCCESS);
}

RenderPass::~RenderPass()
{
	vkDestroyRenderPass(device.device, renderPass, nullptr);
}

}
}