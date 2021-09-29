#pragma once

#include "definition.h"

namespace GPU
{

class DeviceVulkan;
class Fence;

struct FenceDeleter
{
    void operator()(Fence* fence);
};
class Fence : public Util::IntrusivePtrEnabled<Fence, FenceDeleter>
{
public:
    Fence(DeviceVulkan& device_, VkFence fence_);
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
    bool isWait = false;
};
using FencePtr = Util::IntrusivePtr<Fence>;

class FenceManager
{
public:
    FenceManager();
    ~FenceManager();

    void Initialize(DeviceVulkan& device_);
    VkFence Requset();
    void Recyle(VkFence fence);
    void ClearAll();

private:
    DeviceVulkan* device = nullptr;
    std::vector<VkFence> fences;
};

}