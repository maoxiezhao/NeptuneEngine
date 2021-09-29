#pragma once

#include "definition.h"

namespace GPU
{

class DeviceVulkan;
class Semaphore;

struct SemaphoreDeleter
{
    void operator()(Semaphore* semaphore);
};
class Semaphore : public Util::IntrusivePtrEnabled<Semaphore, SemaphoreDeleter>
{
public:
    Semaphore(DeviceVulkan& device_, VkSemaphore semaphore_, bool isSignalled_);
    ~Semaphore();

    const VkSemaphore& GetSemaphore()const
    {
        return semaphore;
    }

    bool IsSignalled() const
    {
        return signalled;
    }

    uint64_t GetTimeLine()const
    {
        return timeline;
    }

    VkSemaphore Release()
    {
        auto oldSemaphore = semaphore;
        semaphore = VK_NULL_HANDLE;
        signalled = false;
        return oldSemaphore;
    }

    void Signal()
    {
        assert(!signalled);
        signalled = true;
    }

private:
    friend class DeviceVulkan;
    friend struct SemaphoreDeleter;
    friend class Util::ObjectPool<Semaphore>;

    DeviceVulkan& device;
    VkSemaphore semaphore;
    bool signalled = true;
    uint64_t timeline = 0;
};
using SemaphorePtr = Util::IntrusivePtr<Semaphore>;

class SemaphoreManager
{
public:
    ~SemaphoreManager();

    void Initialize(DeviceVulkan& device_);
    VkSemaphore Requset();
    void Recyle(VkSemaphore semaphore);
    void ClearAll();

private:
    DeviceVulkan* device = nullptr;
    std::vector<VkSemaphore> seamphores;
};

}