#pragma once

#include "definition.h"
#include "renderPass.h"

namespace GPU
{

class FrameBuffer;

class FrameBuffer : public HashedObject<FrameBuffer>
{
public:
	FrameBuffer(DeviceVulkan& device_, RenderPass& renderPass_, const RenderPassInfo& info_);
	~FrameBuffer();

	FrameBuffer(const FrameBuffer&) = delete;
	void operator=(const FrameBuffer&) = delete;

	uint32_t GetWidth()
	{
		return width;
	}

	uint32_t GetHeight()
	{
		return height;
	}

	VkFramebuffer GetFrameBuffer()
	{
		return frameBuffer;
	}

	const RenderPass& GetRenderPass()const
	{
		return renderPass;
	}

private:
	friend struct FrameBufferDeleter;

	DeviceVulkan& device;
	VkFramebuffer frameBuffer = VK_NULL_HANDLE;
	uint32_t width = 0;
	uint32_t height = 0;
	const RenderPass& renderPass;
};

}