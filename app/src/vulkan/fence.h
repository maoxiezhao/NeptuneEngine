#pragma once

#include "definition.h"

class DeviceVulkan;
class Fence;

struct FenceDeleter
{
    void operator()(Fence* fence);
};
class Fence : public Util::IntrusivePtrEnabled<Fence, FenceDeleter>
{
public:
    Fence(DeviceVulkan& device, VkFence fence);
    ~Fence();

    VkFence GetFence()const
    {
        return mFence;
    }

    void Wait();

private:
    friend class DeviceVulkan;
    friend struct FenceDeleter;
    friend class Util::ObjectPool<Fence>;

    DeviceVulkan& mDevice;
    VkFence mFence;
    bool isWait = false;
};
using FencePtr = Util::IntrusivePtr<Fence>;

class FenceManager
{
public:
    FenceManager();
    ~FenceManager();

    void Initialize(DeviceVulkan& device);
    VkFence Requset();
    void Recyle(VkFence fence);
    void ClearAll();

private:
    DeviceVulkan* mDevice = nullptr;
    std::vector<VkFence> mFences;
};