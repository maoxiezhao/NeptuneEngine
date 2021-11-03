#include "memory.h"
#include "device.h"

#define VMA_IMPLEMENTATION
#include "utility\vk_mem_alloc.h"

namespace GPU
{
	namespace {
	
		VmaMemoryUsage GetMemoryUsage(AllocationMode mode)
		{
			VmaMemoryUsage ret = VMA_MEMORY_USAGE_UNKNOWN;
			switch (mode)
			{
			case AllocationMode::Device:
				ret = VMA_MEMORY_USAGE_GPU_ONLY;
				break;
			case AllocationMode::LinkedDeviceHost:
				ret = VMA_MEMORY_USAGE_CPU_TO_GPU;
				break;
			case AllocationMode::Host:
				ret = VMA_MEMORY_USAGE_CPU_ONLY;
				break;
			case AllocationMode::CachedHost:
				ret = VMA_MEMORY_USAGE_GPU_TO_CPU;
				break;
			}
			return ret;
		}

		VmaAllocationCreateFlags GetCreateFlags(AllocationMode mode)
		{
			VmaAllocationCreateFlags ret = 0;
			switch (mode)
			{
			case AllocationMode::Host:
			case AllocationMode::CachedHost:
				ret = VMA_ALLOCATION_CREATE_MAPPED_BIT;
				break;
			}
			return ret;
		}
	}

	void DeviceAllocation::Free(DeviceAllocator& allocator)
	{
		if (allocation != VK_NULL_HANDLE)
			vmaFreeMemory(allocator.allocator, allocation);
	}

	void DeviceAllocationOwnerDeleter::operator()(DeviceAllocationOwner* owner)
	{
		owner->device.allocations.free(owner);
	}

	DeviceAllocationOwner::~DeviceAllocationOwner()
	{
		if (allocation.GetMemory() != VK_NULL_HANDLE)
			device.FreeMemory(allocation);
	}

	const DeviceAllocation DeviceAllocationOwner::GetAllocatoin() const
	{
		return allocation;
	}

	DeviceAllocationOwner::DeviceAllocationOwner(DeviceVulkan& device_, DeviceAllocation allocation_) :
		device(device_),
		allocation(allocation_)
	{
	}

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
		allocInfo.usage = GetMemoryUsage(mode);
		allocInfo.flags = GetCreateFlags(mode);
		return vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation->allocation, nullptr) != VK_SUCCESS;
	}

	bool DeviceAllocator::Allocate(U32 size, U32 alignment, U32 typeBits, AllocationMode mode, DeviceAllocation* allocation)
	{
		VkMemoryRequirements memRep = {};
		memRep.size = size;
		memRep.alignment = alignment;
		memRep.memoryTypeBits = typeBits;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = GetMemoryUsage(mode);
		allocInfo.flags = GetCreateFlags(mode);

		return vmaAllocateMemory(allocator, &memRep, &allocInfo, &allocation->allocation, nullptr) != VK_SUCCESS;
	}

	void* DeviceAllocator::MapMemory(DeviceAllocation& allocation)
	{
		void* data = nullptr;
		if (vmaMapMemory(allocator, allocation.GetMemory(), &data) != VK_SUCCESS)
			return nullptr;

		return data;
	}

	void DeviceAllocator::UnmapMemory(DeviceAllocation& allocation)
	{
		vmaUnmapMemory(allocator, allocation.allocation);
	}
}