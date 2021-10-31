#include "buffer.h"
#include "device.h"

namespace GPU
{
	void BufferViewDeleter::operator()(BufferView* bufferView)
	{
		bufferView->device.bufferViews.free(bufferView);
	}

	BufferView::BufferView(DeviceVulkan& device_, VkBufferView view_, const BufferViewCreateInfo& info_) :
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
		device(device_),
		buffer(buffer_),
		allocation(allocation_),
		info(info_)
	{
	}
}