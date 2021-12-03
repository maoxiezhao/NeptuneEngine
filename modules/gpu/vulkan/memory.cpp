#include "memory.h"
#include "device.h"

#define VMA_IMPLEMENTATION
#include "utility\vk_mem_alloc.h"

namespace VulkanTest
{
namespace GPU
{
	namespace {
	
		VmaMemoryUsage GetMemoryUsage(BufferDomain domain)
		{
			VmaMemoryUsage ret = VMA_MEMORY_USAGE_UNKNOWN;
			switch (domain)
			{
			case BufferDomain::Device:
				ret = VMA_MEMORY_USAGE_GPU_ONLY;
				break;
			case BufferDomain::LinkedDeviceHost:
				ret = VMA_MEMORY_USAGE_CPU_TO_GPU;
				break;
			case BufferDomain::Host:
				ret = VMA_MEMORY_USAGE_CPU_ONLY;
				break;
			case BufferDomain::CachedHost:
				ret = VMA_MEMORY_USAGE_GPU_TO_CPU;
				break;
			}
			return ret;
		}
		VmaAllocationCreateFlags GetCreateFlags(BufferDomain domain)
		{
			VmaAllocationCreateFlags ret = 0;
			switch (domain)
			{
			case BufferDomain::Host:
			case BufferDomain::CachedHost:
				ret |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
				break;
			}
			return ret;
		}

		VmaMemoryUsage GetMemoryUsage(ImageDomain domain)
		{
			VmaMemoryUsage ret = VMA_MEMORY_USAGE_UNKNOWN;
			switch (domain)
			{
			case ImageDomain::Physical:
				ret = VMA_MEMORY_USAGE_GPU_ONLY;
				break;
			case ImageDomain::Transient:
				ret = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
				break;
			case ImageDomain::LinearHostCached:
				ret = VMA_MEMORY_USAGE_GPU_TO_CPU;
				break;
			case ImageDomain::LinearHost:
				ret = VMA_MEMORY_USAGE_CPU_ONLY;
				break;
			}
			return ret;
		}
		VmaAllocationCreateFlags GetCreateFlags(ImageDomain domain)
		{
			VmaAllocationCreateFlags ret = 0;
			switch (domain)
			{
			case ImageDomain::LinearHost:
			case ImageDomain::LinearHostCached:
				ret |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
				break;
			}
			return ret;
		}
	}

	bool DeviceAllocation::IsHostVisible() const
	{
		return memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
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

	bool DeviceAllocator::CreateBuffer(const VkBufferCreateInfo& bufferInfo, BufferDomain domain, VkBuffer& buffer, DeviceAllocation* allocation)
	{
		VmaAllocationInfo allocInfo;
		VmaAllocationCreateInfo createInfo = {};
		createInfo.usage = GetMemoryUsage(domain);
		createInfo.flags = GetCreateFlags(domain);
		bool ret = vmaCreateBuffer(allocator, &bufferInfo, &createInfo, &buffer, &allocation->allocation, &allocInfo) == VK_SUCCESS;
		if (ret == true)
		{
			VkMemoryPropertyFlags memFlags;
			vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &memFlags);
			if ((memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 && allocInfo.pMappedData != nullptr)
				allocation->hostBase = static_cast<U8*>(allocInfo.pMappedData);

			allocation->memFlags = memFlags;
		}
		return ret;
	}

	bool DeviceAllocator::CreateImage(const VkImageCreateInfo& imageInfo, ImageDomain domain, VkImage& image, DeviceAllocation* allocation)
	{
		VmaAllocationInfo allocInfo;
		VmaAllocationCreateInfo createInfo = {};
		createInfo.usage = GetMemoryUsage(domain);
		createInfo.flags = GetCreateFlags(domain);
		bool ret = vmaCreateImage(allocator, &imageInfo, &createInfo, &image, &allocation->allocation, &allocInfo) == VK_SUCCESS;
		if (ret == true)
		{
			VkMemoryPropertyFlags memFlags;
			vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &memFlags);
			if ((memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 && allocInfo.pMappedData != nullptr)
				allocation->hostBase = static_cast<U8*>(allocInfo.pMappedData);

			allocation->memFlags = memFlags;
		}
		return ret;
	}


	bool DeviceAllocator::Allocate(U32 size, U32 alignment, U32 typeBits, const MemoryAllocateUsage& usage, DeviceAllocation* allocation)
	{
		VkMemoryRequirements memRep = {};
		memRep.size = size;
		memRep.alignment = alignment;
		memRep.memoryTypeBits = typeBits;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage.usage;
		allocCreateInfo.flags = usage.flags;

		VmaAllocationInfo allocInfo;
		bool ret = vmaAllocateMemory(allocator, &memRep, &allocCreateInfo, &allocation->allocation, &allocInfo) == VK_SUCCESS;
		if (ret == true)
		{
			VkMemoryPropertyFlags memFlags;
			vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &memFlags);
			if ((memFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0 && allocInfo.pMappedData != nullptr)
				allocation->hostBase = static_cast<U8*>(allocInfo.pMappedData);

			allocation->memFlags = memFlags;
		}
		return ret;
	}

	void* DeviceAllocator::MapMemory(const DeviceAllocation& allocation, MemoryAccessFlags flags, VkDeviceSize offset, VkDeviceSize length)
	{
		if (allocation.hostBase == nullptr)
			return nullptr;

		if (flags & MEMORY_ACCESS_READ_BIT && !(allocation.memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			vmaInvalidateAllocation(allocator, allocation.allocation, offset, length);

		return allocation.hostBase + offset;
	}

	void DeviceAllocator::UnmapMemory(const DeviceAllocation& allocation, MemoryAccessFlags flags, VkDeviceSize offset, VkDeviceSize length)
	{
		if (allocation.hostBase == nullptr)
			return;

		if (flags & MEMORY_ACCESS_WRITE_BIT && !(allocation.memFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			vmaFlushAllocation(allocator, allocation.allocation, offset, length);
	}
}
}