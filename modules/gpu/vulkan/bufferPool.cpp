#include "bufferPool.h"
#include "device.h"

namespace VulkanTest
{
namespace GPU
{
    BufferPool::BufferPool()
    {
    }

    BufferPool::~BufferPool()
    {
        ASSERT(blocks.empty());
    }

    void BufferPool::Init(DeviceVulkan* device_, VkDeviceSize blockSize_, VkDeviceSize alignment_, VkBufferUsageFlags usage_, U32 maxRetainedBlocks_)
    {
        device = device_;
        blockSize = blockSize_;
        alignment = alignment_;
        usage = usage_;
        maxRetainedBlocks = maxRetainedBlocks_;
    }

    void BufferPool::Reset()
    {
        blocks.clear();
    }

    BufferBlock BufferPool::RequestBlock(VkDeviceSize minimumSize)
    {
        if ((minimumSize > blockSize) || blocks.empty())
            return AllocateBlock(std::max(blockSize, minimumSize));

        auto back = std::move(blocks.back());
        blocks.pop_back();
        back.mapped = static_cast<U8*>(device->MapBuffer(*back.cpu, MEMORY_ACCESS_WRITE_BIT));
        back.offset = 0;
        return back;
    }

    void BufferPool::RecycleBlock(BufferBlock& block)
    {
        ASSERT(block.capacity == blockSize);

        if (blocks.size() >= maxRetainedBlocks)
        {
            block = {};
            return;
        }
        blocks.push_back(std::move(block));
    }

    BufferBlock BufferPool::AllocateBlock(VkDeviceSize size)
    {
        BufferCreateInfo info = {};
        info.domain = ((usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0) ? BufferDomain::Host : BufferDomain::LinkedDeviceHost;
        info.size = size;
        info.usage = usage;

        BufferBlock block;
        block.gpu = device->CreateBuffer(info, nullptr);
        block.gpu->SetInternalSyncObject();
        device->SetName(*block.gpu, "Allocated_block_gpu");
        
        block.mapped = static_cast<U8*>(device->MapBuffer(*block.gpu, MEMORY_ACCESS_WRITE_BIT));
        if (block.mapped == nullptr)
        {
            // Create a cpu buffer for trasnfer to gpu
            BufferCreateInfo info = {};
            info.domain = BufferDomain::Host;
            info.size = size;
            info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            block.cpu = device->CreateBuffer(info, nullptr);
            block.cpu->SetInternalSyncObject();
            device->SetName(*block.gpu, "Allocated_block_cpu");
            block.mapped = static_cast<U8*>(device->MapBuffer(*block.cpu, MEMORY_ACCESS_WRITE_BIT));
        }
        else
        {
            block.cpu = block.gpu;
        }

        block.offset = 0;
        block.alignment = alignment;
        block.spillSize = spillSize;
        block.capacity = size;
        return block;
    }
}
}