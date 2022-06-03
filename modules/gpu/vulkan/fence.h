#pragma once

#include "definition.h"

namespace VulkanTest
{
namespace GPU
{

class DeviceVulkan;
class Fence;

struct FenceDeleter
{
    void operator()(Fence* fence);
};
class Fence : public IntrusivePtrEnabled<Fence, FenceDeleter>, public InternalSyncObject
{
public:
    Fence(DeviceVulkan& device_, VkFence fence_);
    Fence(DeviceVulkan& device_, U64 timeline_, VkSemaphore timelineSemaphore_);
    ~Fence();

    VkFence GetFence()const
    {
        return fence;
    }

    void Wait();

private:
    friend class DeviceVulkan;
    friend struct FenceDeleter;
    friend class Util::ObjectPool<Fence>;

    DeviceVulkan& device;
    VkFence fence;
    VkSemaphore timelineSemaphore;
    uint64_t timeline;
    bool isWaiting = false;
    std::mutex lock;
};
using FencePtr = IntrusivePtr<Fence>;

class FenceManager
{
public:
    FenceManager();
    ~FenceManager();

    void Initialize(DeviceVulkan& device_);
    VkFence Requset();
    void Recyle(VkFence fence);

private:
    DeviceVulkan* device = nullptr;
    std::vector<VkFence> fences;
};

}
}