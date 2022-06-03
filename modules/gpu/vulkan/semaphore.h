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
class Semaphore : public IntrusivePtrEnabled<Semaphore, SemaphoreDeleter>, public InternalSyncObject
{
public:
    Semaphore(DeviceVulkan& device_, VkSemaphore semaphore_ = VK_NULL_HANDLE, bool isSignalled_ = true);
    Semaphore(DeviceVulkan& device_, U64 timeline_, VkSemaphore semaphore_);
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
        ASSERT(semaphore);
        ASSERT(signalled);
        semaphore = VK_NULL_HANDLE;
        signalled = false;
        return oldSemaphore;
    }

    void WaitExternal()
    {
        ASSERT(signalled);
        signalled = false;
    }

    void Signal()
    {
        ASSERT(!signalled);
        signalled = true;
    }

    void SignalPendingWait()
    {
        ASSERT(isPendingWait == false);
        isPendingWait = true;
    }

    bool IsPendingWait()const
    {
        return isPendingWait;
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
    bool isPendingWait = false;
    bool shouldDestroyOnConsume = false;
};
using SemaphorePtr = IntrusivePtr<Semaphore>;

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