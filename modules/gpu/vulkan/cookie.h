#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{
    class DeviceVulkan;
    class GraphicsCookie
    {
    public:
        GraphicsCookie(DeviceVulkan& device);

        uint64_t GetCookie() const
        {
            return cookie;
        }

    private:
        uint64_t cookie;
    };
}
}