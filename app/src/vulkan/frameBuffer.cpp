#include "frameBuffer.h"
#include "device.h"

FrameBuffer::FrameBuffer(DeviceVulkan& device, RenderPass& renderPass, const RenderPassInfo& info) :
	mDevice(device)
{
	VkImageView imageViews[VULKAN_NUM_ATTACHMENTS + 1];
	uint32_t numImageViews = 0;
	mWidth = UINT32_MAX;
	mHeight = UINT32_MAX;

	// 遍历所有ColorAttachments，获取imageView，获取最小的size(w, h)
	for (auto& view : info.mColorAttachments)
	{
		imageViews[numImageViews++] = view->GetRenderTargetView(0);
		mWidth = min(mWidth, view->GetImage()->GetWidth());
		mHeight = min(mHeight, view->GetImage()->GetHeight());
	}

	if (info.mDepthStencil != nullptr)
	{
		imageViews[numImageViews++] = info.mDepthStencil->GetRenderTargetView(0);
		mWidth = min(mWidth, info.mDepthStencil->GetImage()->GetWidth());
		mHeight = min(mHeight, info.mDepthStencil->GetImage()->GetHeight());
	}

	VkFramebufferCreateInfo framebufferInfo = {};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass.GetRenderPass();
	framebufferInfo.attachmentCount = numImageViews;
	framebufferInfo.pAttachments = imageViews;
	framebufferInfo.width = mWidth;
	framebufferInfo.height = mHeight;
	framebufferInfo.layers = 1;

	VkResult res = vkCreateFramebuffer(mDevice.mDevice, &framebufferInfo, nullptr, &mFrameBuffer);
	assert(res == VK_SUCCESS);
}

FrameBuffer::~FrameBuffer()
{
	if (mFrameBuffer != VK_NULL_HANDLE)
	{
		mDevice.ReleaseFrameBuffer(mFrameBuffer);
	}
}