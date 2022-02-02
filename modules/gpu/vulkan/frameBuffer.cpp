#include "frameBuffer.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{

FrameBuffer::FrameBuffer(DeviceVulkan& device_, RenderPass& renderPass, const RenderPassInfo& info) :
	GraphicsCookie(device_),
	device(device_),
	renderPass(renderPass)
{
	VkImageView imageViews[VULKAN_NUM_ATTACHMENTS + 1];
	uint32_t numImageViews = 0;
	width = UINT32_MAX;
	height = UINT32_MAX;

	for (U32 i = 0; i < info.numColorAttachments; i++)
	{
		const ImageView* view = info.colorAttachments[i];
		imageViews[numImageViews++] = view->GetRenderTargetView(0);
		width = std::min(width, view->GetImage()->GetWidth());
		height = std::min(height, view->GetImage()->GetHeight());
	}

	if (info.depthStencil != nullptr)
	{
		imageViews[numImageViews++] = info.depthStencil->GetRenderTargetView(0);
		width = std::min(width, info.depthStencil->GetImage()->GetWidth());
		height = std::min(height, info.depthStencil->GetImage()->GetHeight());
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
		if (internalSync)
			device.ReleaseFrameBufferNolock(frameBuffer);
		else
			device.ReleaseFrameBuffer(frameBuffer);
	}
}

}
}