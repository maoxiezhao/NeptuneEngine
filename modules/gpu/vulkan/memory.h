#pragma once

#include "definition.h"

#define VK_NO_PROTOTYPES
#include "utility\vk_mem_alloc.h"

namespace GPU
{
	class DeviceVulkan;
	class DeviceAllocator;

	enum class AllocationMode : uint8_t
	{
		Host,
		Device,
		Count
	};

	struct DeviceAllocation
	{
	public:
		VmaAllocation allocation = VK_NULL_HANDLE;
		uint32_t offset = 0;
		uint32_t mask = 0;
		uint32_t size = 0;

	public:
		void Free(DeviceAllocator& allocator);
	};

	class DeviceAllocator
	{
	public:
		DeviceAllocator();
		~DeviceAllocator();

		void Initialize(DeviceVulkan* device_);
		bool CreateBuffer(const VkBufferCreateInfo& bufferInfo, AllocationMode mode, VkBuffer& buffer, DeviceAllocation* allocation);

	private:
		friend struct DeviceAllocation;

		DeviceVulkan* device;
		VmaAllocator allocator = VK_NULL_HANDLE;
	};
}