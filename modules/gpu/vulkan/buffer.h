#pragma once

#include "definition.h"
#include "memory.h"

namespace VulkanTest
{
namespace GPU
{
    class Buffer;
    class BufferView;

    struct BufferViewDeleter
    {
        void operator()(BufferView* bufferView);
    };
    class BufferView : public Util::IntrusivePtrEnabled<BufferView, BufferViewDeleter>
    {
    public:
        BufferView(DeviceVulkan& device_, VkBufferView view_, const BufferViewCreateInfo& info_);
        ~BufferView();

        VkBufferView GetBufferView()
        {
            return view;
        }

    private:
        friend class DeviceVulkan;
        friend struct BufferViewDeleter;
        friend class Util::ObjectPool<BufferView>;

        DeviceVulkan& device;
        VkBufferView view;
        BufferViewCreateInfo info;
    };
    using BufferViewPtr = Util::IntrusivePtr<BufferView>;

    struct BufferDeleter
    {
        void operator()(Buffer* buffer);
    };
    class Buffer : public Util::IntrusivePtrEnabled<Buffer, BufferDeleter>
    {
    public:
        ~Buffer();
  
        VkBuffer GetBuffer()const
        {
            return buffer;
        }

        const BufferCreateInfo& GetCreateInfo()const
        {
            return info;
        }

        DeviceAllocation& GetAllcation()
        {
            return allocation;
        }

        const DeviceAllocation& GetAllcation()const
        {
            return allocation;
        }

        static VkPipelineStageFlags BufferUsageToPossibleStages(VkBufferUsageFlags usage);

    private:
        friend class DeviceVulkan;
        friend struct BufferDeleter;
        friend class Util::ObjectPool<Buffer>;

        Buffer(DeviceVulkan& device_, VkBuffer buffer_, const DeviceAllocation& allocation_, const BufferCreateInfo& info_);

    private:
        DeviceVulkan& device;
        VkBuffer buffer;
        DeviceAllocation allocation;
        BufferCreateInfo info;
    };
    using BufferPtr = Util::IntrusivePtr<Buffer>;
}
}