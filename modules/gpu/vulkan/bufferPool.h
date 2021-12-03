#pragma once

#include "definition.h"
#include "buffer.h"

namespace VulkanTest
{
namespace GPU
{
	class DeviceVulkan;

	struct BufferBlockAllocation
	{
		U8* data;
		VkDeviceSize offset;
		VkDeviceSize paddedSize;
	};

	struct BufferBlock
	{
	public:
		U8* mapped = nullptr;
		VkDeviceSize offset = 0;
		VkDeviceSize alignment = 0;
		VkDeviceSize container;

		BufferPtr cpuBuffer;

	public:
		BufferBlockAllocation Allocate(VkDeviceSize allocateSize)
		{
			VkDeviceSize aligned = (offset + alignment - 1) & ~(alignment - 1);
			if (aligned + allocateSize <= container)
			{
				U8* ret = mapped + aligned;
				offset = aligned + allocateSize;

				VkDeviceSize paddedSize = std::min(paddedSize, container - aligned);
				return { ret , allocateSize, paddedSize };
			}

			return { nullptr, 0, 0 };
		}

		bool IsAllocated()const
		{
			return cpuBuffer.operator bool();
		}
	};

	class BufferPool
	{
	public:
		BufferPool();
		~BufferPool();

		void Init(DeviceVulkan* device_, VkDeviceSize blockSize_, VkDeviceSize alignment_, VkBufferUsageFlags usage_, U32 maxRetainedBlocks_ = 0);
		void Reset();
		BufferBlock RequestBlock(VkDeviceSize minimumSize);
		void RecycleBlock(BufferBlock& block);

		VkDeviceSize GetBlockSize()const {
			return blockSize;
		}

	private:
		BufferBlock AllocateBlock(VkDeviceSize size);

		DeviceVulkan* device;
		VkDeviceSize blockSize;
		VkDeviceSize alignment;
		VkBufferUsageFlags usage;
		U32 maxRetainedBlocks;
		std::vector<BufferBlock> blocks;
	};
}
}