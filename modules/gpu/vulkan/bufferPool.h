#pragma once

#include "definition.h"
#include "buffer.h"
#include "descriptorSet.h"

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
		BindlessDescriptorPtr bindless;
	};

	struct BufferBlock
	{
	public:
		U8* mapped = nullptr;
		VkDeviceSize offset = 0;
		VkDeviceSize alignment = 0;
		VkDeviceSize capacity = 0;
		VkDeviceSize spillSize = 0;
		BufferPtr gpu;
		BufferPtr cpu;
		BindlessDescriptorPtr bindless;

	public:
		BufferBlockAllocation Allocate(VkDeviceSize allocateSize)
		{
			VkDeviceSize alignedOffset = (offset + alignment - 1) & ~(alignment - 1);
			if (alignedOffset + allocateSize <= capacity)
			{
				U8* ret = mapped + alignedOffset;
				offset = alignedOffset + allocateSize;

				VkDeviceSize paddedSize = std::max(allocateSize, spillSize);
				paddedSize = std::min(paddedSize, capacity - alignedOffset);
				return { ret, alignedOffset, paddedSize, bindless };
			}

			return { nullptr, 0, 0, bindless };
		}
	};

	class BufferPool
	{
	public:
		BufferPool();
		~BufferPool();

		void Init(DeviceVulkan* device_, VkDeviceSize blockSize_, VkDeviceSize alignment_, VkBufferUsageFlags usage_, U32 maxRetainedBlocks_);
		void Reset();
		BufferBlock RequestBlock(VkDeviceSize minimumSize);
		void RecycleBlock(BufferBlock& block);

		VkDeviceSize GetBlockSize()const {
			return blockSize;
		}

		void SetSpillSize(VkDeviceSize spillSize_) {
			spillSize = spillSize_;
		}
		
		void AllocateBindlessDescriptor(bool allocated){
			allocateBindlessDescriptor = allocated;
		}

	private:
		BufferBlock AllocateBlock(VkDeviceSize size);

		DeviceVulkan* device = nullptr;
		VkDeviceSize blockSize = 0;
		VkDeviceSize alignment= 0 ;
		VkDeviceSize spillSize = 0;
		VkBufferUsageFlags usage = 0;
		U32 maxRetainedBlocks = 0;
		bool allocateBindlessDescriptor = false;
		std::vector<BufferBlock> blocks;
	};
}
}