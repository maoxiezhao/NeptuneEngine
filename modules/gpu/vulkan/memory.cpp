#include "memory.h"
#include "device.h"

#define VMA_IMPLEMENTATION
#include "utility\vk_mem_alloc.h"

namespace GPU
{
	DeviceAllocator::DeviceAllocator() :
		device(nullptr)
	{
	}

	DeviceAllocator::~DeviceAllocator()
	{
		if (allocator != VK_NULL_HANDLE)
			vmaDestroyAllocator(allocator);
	}

	void DeviceAllocator::Initialize(DeviceVulkan* device_)
	{
		device = device_;
		allocator = VK_NULL_HANDLE;

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = device->physicalDevice;
		allocatorInfo.device = device->device;
		allocatorInfo.instance = device->instance;
		if (device->features.features_1_2.bufferDeviceAddress)
			allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

		if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
		{
			Logger::Error("Failed to initialize memory allocator.");
			return;
		}
	}

	bool DeviceAllocator::CreateBuffer(const VkBufferCreateInfo& bufferInfo, AllocationMode mode, VkBuffer& buffer, DeviceAllocation* allocation)
	{
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (mode == AllocationMode::Host)
		{
			allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
			allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY; // yes, not using VMA_MEMORY_USAGE_CPU_TO_GPU, as it had worse performance for CPU write
		}
		else if (mode == AllocationMode::Device)
		{
			allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
			allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
		}

		return vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation->allocation, nullptr) != VK_SUCCESS;
	}

	void DeviceAllocation::Free(DeviceAllocator& allocator)
	{
		if (allocation != VK_NULL_HANDLE)
			vmaFreeMemory(allocator.allocator, allocation);
	}
}