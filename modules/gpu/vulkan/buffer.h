#pragma once

#include "definition.h"
#include "memory.h"
#include "cookie.h"

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
    class BufferView : public IntrusivePtrEnabled<BufferView, BufferViewDeleter>, public GraphicsCookie, public InternalSyncObject
    {
    public:
        BufferView(DeviceVulkan& device_, VkBufferView view_, const BufferViewCreateInfo& info_);
        ~BufferView();

        VkBufferView GetBufferView() const {
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
    using BufferViewPtr = IntrusivePtr<BufferView>;

    struct BufferDeleter
    {
        void operator()(Buffer* buffer);
    };
    class Buffer : public IntrusivePtrEnabled<Buffer, BufferDeleter>, public GraphicsCookie, public InternalSyncObject
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
        static VkAccessFlags BufferUsageToPossibleAccess(VkBufferUsageFlags usage);

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
    using BufferPtr = IntrusivePtr<Buffer>;
}
}