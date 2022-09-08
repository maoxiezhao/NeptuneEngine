#pragma once

#include "definition.h"

#define VK_NO_PROTOTYPES
#include "utility\vk_mem_alloc.h"

namespace VulkanTest
{
namespace GPU
{
	class DeviceVulkan;
	class DeviceAllocator;

	enum MemoryAccessFlag : U32
	{
		MEMORY_ACCESS_WRITE_BIT = 1,
		MEMORY_ACCESS_READ_BIT = 2,
		MEMORY_ACCESS_READ_WRITE_BIT = MEMORY_ACCESS_WRITE_BIT | MEMORY_ACCESS_READ_BIT
	};
	using MemoryAccessFlags = U32;

	struct MemoryUsage
	{
		U64 total = 0ull;		// Total video memory (bytes)
		U64 usage = 0ull;		// Used video memory  (bytes)
	};

	struct MemoryAllocateUsage
	{
		VmaMemoryUsage usage;
		VmaAllocationCreateFlags flags;
	};

	struct MemoryAllocateInfo
	{
		VkMemoryRequirements requirements = {};
		VkMemoryPropertyFlags requiredProperties = 0;
		MemoryAllocateUsage usage;
	};

	struct DeviceAllocation
	{
	public:
		VmaAllocation allocation = VK_NULL_HANDLE;
		U32 offset = 0;
		U32 mask = 0;
		U32 size = 0;
		U8* hostBase = nullptr;
		VkMemoryPropertyFlags memFlags = 0;

	public:
		VmaAllocation GetMemory()const
		{
			return allocation;
		}

		U32 GetOffset()const
		{
			return offset;
		}

		U32 GetMask()const
		{
			return mask;
		}

		U32 GetSize()const
		{
			return size;
		}

		bool IsHostVisible()const;

		void Free(DeviceAllocator& allocator);
	};

	class DeviceAllocationOwner;
	struct DeviceAllocationOwnerDeleter
	{
		void operator()(DeviceAllocationOwner* owner);
	};
	class DeviceAllocationOwner : public Util::IntrusiveHashMapEnabled<DeviceAllocationOwner>, public IntrusivePtrEnabled<DeviceAllocationOwner, DeviceAllocationOwnerDeleter>
	{
	public:
		~DeviceAllocationOwner();

		const DeviceAllocation GetAllocatoin()const;

	private:
		friend class DeviceVulkan;
		friend struct DeviceAllocationOwnerDeleter;
		friend class Util::ObjectPool<DeviceAllocationOwner>;

		DeviceAllocationOwner(DeviceVulkan& device_, DeviceAllocation allocation_);
		DeviceVulkan& device;
		DeviceAllocation allocation;
	};
	using DeviceAllocationOwnerPtr = IntrusivePtr<DeviceAllocationOwner>;

	class DeviceAllocator
	{
	public:
		DeviceAllocator();
		~DeviceAllocator();

		void Initialize(DeviceVulkan* device_);
		bool CreateBuffer(const VkBufferCreateInfo& bufferInfo, BufferDomain domain, VkBuffer& buffer, DeviceAllocation* allocation);
		bool CreateImage(const VkImageCreateInfo& imageInfo, ImageDomain domain, VkImage& image, DeviceAllocation* allocation);
		bool Allocate(U32 size, U32 alignment, U32 typeBits, const MemoryAllocateUsage& usage, DeviceAllocation* allocation);
		void* MapMemory(const DeviceAllocation& allocation, MemoryAccessFlags flags, VkDeviceSize offset, VkDeviceSize length);
		void UnmapMemory(const DeviceAllocation& allocation, MemoryAccessFlags flags, VkDeviceSize offset, VkDeviceSize length);

		MemoryUsage GetMemoryUsage()const;

	private:
		friend struct DeviceAllocation;

		DeviceVulkan* device;
		VmaAllocator allocator = VK_NULL_HANDLE;
		VkPhysicalDeviceMemoryProperties2 memoryProperties2 = {};
	};
}
}