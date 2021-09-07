#include "renderPass.h"
#include "gpu.h"

RenderPass::RenderPass(DeviceVulkan& device, const RenderPassInfo& info) :
	mDevice(device)
{
	// Color attachments
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = VK_FORMAT_UNDEFINED; //swapchain->mFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo createRenderPassInfo = {};
	createRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createRenderPassInfo.attachmentCount = 1;
	createRenderPassInfo.pAttachments = &colorAttachment;
	createRenderPassInfo.subpassCount = 1;
	createRenderPassInfo.pSubpasses = &subpass;

	VkResult res = vkCreateRenderPass(device.mDevice, &createRenderPassInfo, nullptr, &mRenderPass);
	assert(res == VK_SUCCESS);
}

RenderPass::~RenderPass()
{
	vkDestroyRenderPass(mDevice.mDevice, mRenderPass, nullptr);
}
