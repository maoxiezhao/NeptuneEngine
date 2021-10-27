#pragma once

#include "definition.h"

namespace GPU
{
    class Buffer;

    struct BufferDeleter
    {
        void operator()(Buffer* buffer);
    };
    class Buffer : public Util::IntrusivePtrEnabled<Buffer, BufferDeleter>
    {
    public:
        ~Buffer();
  
    private:
        friend class DeviceVulkan;
        friend struct BufferDeleter;
        friend class Util::ObjectPool<Buffer>;

        Buffer(DeviceVulkan& device_);

    private:
        DeviceVulkan& device;
        BufferCreateInfo createInfo;
    };
    using BufferPtr = Util::IntrusivePtr<Buffer>;
}