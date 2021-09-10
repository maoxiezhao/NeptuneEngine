#include "renderPass.h"
#include "vulkan/device.h"

namespace {
	VkAttachmentLoadOp CheckLoadOp(const RenderPassInfo& info, uint32_t i)
	{
		if ((info.mClearAttachments & (1u << i)) != 0)
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
		else if ((info.mLoadAttachments & (1u << i)) != 0)
			return  VK_ATTACHMENT_LOAD_OP_LOAD;
		else
			return  VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	}

	VkAttachmentStoreOp CheckStoreOp(const RenderPassInfo& info, uint32_t i)
	{
		if ((info.mStoreAttachments & (1u << i)) != 0)
			return VK_ATTACHMENT_STORE_OP_STORE;
		else
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
	}
}

RenderPass::RenderPass(DeviceVulkan& device, const RenderPassInfo& info) :
	mDevice(device)
{
	uint32_t numAttachments = info.mNumColorAttachments + (info.mDepthStencil != nullptr ? 1 : 0);

	// Color attachments
	VkAttachmentDescription attachments[VULKAN_NUM_ATTACHMENTS + 1];
	for (uint32_t i = 0; i < info.mNumColorAttachments; i++)
	{
		mColorAttachments[i] = info.mColorAttachments[i]->GetFormat();

		auto& attachment = attachments[i];
		attachment.format = mColorAttachments[i];
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = CheckLoadOp(info, i);
		attachment.storeOp = CheckStoreOp(info, i);
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		const Image* image = info.mColorAttachments[i]->GetImage();
		if (image->GetSwapchainLayout() != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			// Keep initial layout
			if (attachment.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD)
				attachment.initialLayout = image->GetSwapchainLayout();

			attachment.finalLayout = image->GetSwapchainLayout();
		}
		else
		{
			attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
		}
	}
	
	// Depth stencil
	if (info.mDepthStencil)
	{
		mDepthStencil = info.mDepthStencil->GetFormat();

		auto& attachment = attachments[info.mNumColorAttachments + 1];
		attachment.format = mDepthStencil;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // clear
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;	// read/write
		attachment.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	// Subpasses
	uint32_t numSubPasses = info.mSubPasses != nullptr ? info.mNumSubPasses : 1; 

	// TODO
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	std::vector< VkSubpassDescription> subPasses(numSubPasses);
	for (uint32_t i = 0; i < numSubPasses; i++)
	{
		VkSubpassDescription& subpass = subPasses[i];
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
	}

	// Create render pass
	VkRenderPassCreateInfo createRenderPassInfo = {};
	createRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createRenderPassInfo.attachmentCount = numAttachments;
	createRenderPassInfo.pAttachments = attachments;
	createRenderPassInfo.subpassCount = numSubPasses;
	createRenderPassInfo.pSubpasses = subPasses.data();

	VkResult res = vkCreateRenderPass(device.mDevice, &createRenderPassInfo, nullptr, &mRenderPass);
	assert(res == VK_SUCCESS);
}

RenderPass::~RenderPass()
{
	vkDestroyRenderPass(mDevice.mDevice, mRenderPass, nullptr);
}
