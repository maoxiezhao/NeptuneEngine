#include "buffer.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{
	VkPipelineStageFlags Buffer::BufferUsageToPossibleStages(VkBufferUsageFlags usage)
	{
		VkPipelineStageFlags flags = 0;
		if (usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT))
			flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

		if (usage & (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
			flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

		if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
			flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

		if (usage & (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | 
					VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
			flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		return flags;
	}

	VkAccessFlags Buffer::BufferUsageToPossibleAccess(VkBufferUsageFlags usage)
	{
		VkAccessFlags flags = 0;
		if (usage & (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT))
			flags |= VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

		if (usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
			flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

		if (usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
			flags |= VK_ACCESS_INDEX_READ_BIT;

		if (usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)
			flags |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

		if (usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
			flags |= VK_ACCESS_UNIFORM_READ_BIT;

		if (usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
			flags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		return flags;
	}

	void BufferViewDeleter::operator()(BufferView* bufferView)
	{
		bufferView->device.bufferViews.free(bufferView);
	}

	BufferView::BufferView(DeviceVulkan& device_, VkBufferView view_, const BufferViewCreateInfo& info_) :
		GraphicsCookie(device_),
		device(device_),
		view(view_),
		info(info_)
	{
	}

	BufferView::~BufferView()
	{
		if (view != VK_NULL_HANDLE)
			device.ReleaseBufferView(view);
	}

	void BufferDeleter::operator()(Buffer* buffer)
	{
		buffer->device.buffers.free(buffer);
	}

	Buffer::~Buffer()
	{
		device.ReleaseBuffer(buffer);
		device.FreeMemory(allocation);
	}

	Buffer::Buffer(DeviceVulkan& device_, VkBuffer buffer_, const DeviceAllocation& allocation_, const BufferCreateInfo& info_) :
		GraphicsCookie(device_),
		device(device_),
		buffer(buffer_),
		allocation(allocation_),
		info(info_)
	{
	}
}
}