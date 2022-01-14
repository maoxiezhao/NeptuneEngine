#pragma once

#include "definition.h"

namespace VulkanTest
{
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

    VkSemaphore Consume()
    {
        auto oldSemaphore = semaphore;
        assert(semaphore);
        assert(signalled);
        semaphore = VK_NULL_HANDLE;
        signalled = false;
        return oldSemaphore;
    }

    void WaitExternal()
    {
        signalled = false;
    }

    void Signal()
    {
        assert(!signalled);
        signalled = true;
    }

    bool CanRecycle()const
    {
        return !shouldDestroyOnConsume;
    }

    void Recycle();

    Semaphore& operator=(Semaphore&& other) noexcept;

private:
    friend class DeviceVulkan;
    friend struct SemaphoreDeleter;
    friend class Util::ObjectPool<Semaphore>;

    DeviceVulkan& device;
    VkSemaphore semaphore;
    bool signalled = true;
    uint64_t timeline = 0;
    bool shouldDestroyOnConsume = false;
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
}