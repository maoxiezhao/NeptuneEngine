#pragma once

#include "definition.h"
#include "renderPass.h"

class FrameBuffer;

class FrameBuffer : public HashedObject<FrameBuffer>
{
public:
	FrameBuffer(DeviceVulkan& device, RenderPass& renderPass, const RenderPassInfo& info);
	~FrameBuffer();

	FrameBuffer(const FrameBuffer&) = delete;
	void operator=(const FrameBuffer&) = delete;

	uint32_t GetWidth()
	{
		return mWidth;
	}

	uint32_t GetHeight()
	{
		return mHeight;
	}

	VkFramebuffer GetFrameBuffer()
	{
		return mFrameBuffer;
	}

	const RenderPass& GetRenderPass()const
	{
		return mRenderPass;
	}

private:
	friend struct FrameBufferDeleter;

	DeviceVulkan& mDevice;
	VkFramebuffer mFrameBuffer = VK_NULL_HANDLE;
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;
	const RenderPass& mRenderPass;
};