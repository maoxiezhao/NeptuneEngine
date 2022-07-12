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
        ASSERT(semaphoreType == VK_SEMAPHORE_TYPE_TIMELINE_KHR);
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

    VkSemaphoreTypeKHR GetSemaphoreType() const
    {
        return semaphoreType;
    }

    bool CanRecycle()const {
        return true;
    }

    void Recycle();

    Semaphore& operator=(Semaphore&& other) noexcept;

private:
    friend class DeviceVulkan;
    friend struct SemaphoreDeleter;
    friend class Util::ObjectPool<Semaphore>;

    Semaphore(DeviceVulkan& device_, VkSemaphore semaphore_, bool isSignalled_);
    Semaphore(DeviceVulkan& device_, U64 timeline_, VkSemaphore semaphore_);

    explicit Semaphore(DeviceVulkan& device_)
        : device(device_)
    {
    }

    DeviceVulkan& device;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    uint64_t timeline = 0;
    bool signalled = false;
    bool isPendingWait = false;
    VkSemaphoreTypeKHR semaphoreType = VK_SEMAPHORE_TYPE_BINARY_KHR;
};
using SemaphorePtr = IntrusivePtr<Semaphore>;

class SemaphoreManager
{
public:
    ~SemaphoreManager();

    void Initialize(DeviceVulkan& device_);
    VkSemaphore Requset();
    void Recyle(VkSemaphore semaphore);

private:
    DeviceVulkan* device = nullptr;
    std::vector<VkSemaphore> seamphores;
};

}
}