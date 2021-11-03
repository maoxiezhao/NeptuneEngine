#pragma once

#include "definition.h"

#define VK_NO_PROTOTYPES
#include "utility\vk_mem_alloc.h"

namespace GPU
{
	class DeviceVulkan;
	class DeviceAllocator;

	struct DeviceAllocation
	{
	public:
		VmaAllocation allocation = VK_NULL_HANDLE;
		U32 offset = 0;
		U32 mask = 0;
		U32 size = 0;

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

		void Free(DeviceAllocator& allocator);
	};

	class DeviceAllocationOwner;
	struct DeviceAllocationOwnerDeleter
	{
		void operator()(DeviceAllocationOwner* owner);
	};
	class DeviceAllocationOwner : public Util::IntrusivePtrEnabled<DeviceAllocationOwner, DeviceAllocationOwnerDeleter>
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
	using DeviceAllocationOwnerPtr = Util::IntrusivePtr<DeviceAllocationOwner>;

	class DeviceAllocator
	{
	public:
		DeviceAllocator();
		~DeviceAllocator();

		void Initialize(DeviceVulkan* device_);
		bool CreateBuffer(const VkBufferCreateInfo& bufferInfo, AllocationMode mode, VkBuffer& buffer, DeviceAllocation* allocation);
		bool Allocate(U32 size, U32 alignment, U32 typeBits, AllocationMode mode, DeviceAllocation* allocation);
		void* MapMemory(DeviceAllocation& allocation);
		void UnmapMemory(DeviceAllocation& allocation);

	private:
		friend struct DeviceAllocation;

		DeviceVulkan* device;
		VmaAllocator allocator = VK_NULL_HANDLE;
	};
}