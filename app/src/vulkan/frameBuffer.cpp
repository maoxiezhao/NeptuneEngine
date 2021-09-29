#include "frameBuffer.h"
#include "device.h"

namespace GPU
{

FrameBuffer::FrameBuffer(DeviceVulkan& device_, RenderPass& renderPass, const RenderPassInfo& info) :
	device(device_),
	renderPass(renderPass)
{
	VkImageView imageViews[VULKAN_NUM_ATTACHMENTS + 1];
	uint32_t numImageViews = 0;
	width = UINT32_MAX;
	height = UINT32_MAX;

	// 遍历所有ColorAttachments，获取imageView，获取最小的size(w, h)
	for (auto& view : info.colorAttachments)
	{
		imageViews[numImageViews++] = view->GetRenderTargetView(0);
		width = min(width, view->GetImage()->GetWidth());
		height = min(height, view->GetImage()->GetHeight());
	}

	if (info.depthStencil != nullptr)
	{
		imageViews[numImageViews++] = info.depthStencil->GetRenderTargetView(0);
		width = min(width, info.depthStencil->GetImage()->GetWidth());
		height = min(height, info.depthStencil->GetImage()->GetHeight());
	}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass.GetRenderPass();
	framebufferInfo.attachmentCount = numImageViews;
	framebufferInfo.pAttachments = imageViews;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	framebufferInfo.layers = 1;

	VkResult res = vkCreateFramebuffer(device.device, &framebufferInfo, nullptr, &frameBuffer);
	assert(res == VK_SUCCESS);
}

FrameBuffer::~FrameBuffer()
{
	if (frameBuffer != VK_NULL_HANDLE)
	{
		device.ReleaseFrameBuffer(frameBuffer);
	}
}

}