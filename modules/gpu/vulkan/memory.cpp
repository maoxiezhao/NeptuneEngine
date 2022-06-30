#include "memory.h"
#include "device.h"

#define VMA_IMPLEMENTATION
#include "utility\vk_mem_alloc.h"

namespace VulkanTest
{
namespace GPU
{
	namespace {
	
		VmaAllocationCreateFlags GetCreateFlags(BufferDomain domain, const VkBufferCreateInfo& bufferInfo)
		{
			VmaAllocationCreateFlags ret = 0;
			if (domain == BufferDomain::CachedHost)
				ret |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			else if (domain == BufferDomain::Host)
				ret |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			else if (domain == BufferDomain::LinkedDeviceHost)
				ret |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			return ret;
		}

		VmaAllocationCreateFlags GetCreateFlags(ImageDomain domain)
		{
			VmaAllocationCreateFlags ret = 0;
			//if (domain == ImageDomain::LinearHost)
			//	ret |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			//else if (domain == ImageDomain::LinearHostCached)
			//	ret |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
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

		VmaVulkanFunctions vmaVulkanFunc = {};
		vmaVulkanFunc.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vmaVulkanFunc.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
		vmaVulkanFunc.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		vmaVulkanFunc.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		vmaVulkanFunc.vkAllocateMemory = vkAllocateMemory;
		vmaVulkanFunc.vkFreeMemory = vkFreeMemory;
		vmaVulkanFunc.vkMapMemory = vkMapMemory;
		vmaVulkanFunc.vkUnmapMemory = vkUnmapMemory;
		vmaVulkanFunc.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
		vmaVulkanFunc.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
		vmaVulkanFunc.vkBindBufferMemory = vkBindBufferMemory;
		vmaVulkanFunc.vkBindImageMemory = vkBindImageMemory;
		vmaVulkanFunc.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		vmaVulkanFunc.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		vmaVulkanFunc.vkCreateBuffer = vkCreateBuffer;
		vmaVulkanFunc.vkDestroyBuffer = vkDestroyBuffer;
		vmaVulkanFunc.vkCreateImage = vkCreateImage;
		vmaVulkanFunc.vkDestroyImage = vkDestroyImage;
		vmaVulkanFunc.vkCmdCopyBuffer = vkCmdCopyBuffer;

		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.physicalDevice = device->physicalDevice;
		allocatorInfo.device = device->device;
		allocatorInfo.instance = device->instance;

		// Core in 1.1
		allocatorInfo.flags =
			VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT |
			VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;
		vmaVulkanFunc.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
		vmaVulkanFunc.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;

		if (device->features.features_1_2.bufferDeviceAddress)
		{
			allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
			vmaVulkanFunc.vkBindBufferMemory2KHR = vkBindBufferMemory2;
			vmaVulkanFunc.vkBindImageMemory2KHR = vkBindImageMemory2;
		}

		allocatorInfo.pVulkanFunctions = &vmaVulkanFunc;

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
		createInfo.usage = VMA_MEMORY_USAGE_AUTO;
		createInfo.flags = GetCreateFlags(domain, bufferInfo);
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
		createInfo.usage = VMA_MEMORY_USAGE_AUTO; // GetMemoryUsage(domain);
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