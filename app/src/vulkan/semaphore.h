#pragma once

#include "definition.h"

class DeviceVulkan;
class Semaphore;

struct SemaphoreDeleter
{
    void operator()(Semaphore* semaphore);
};
class Semaphore : public Util::IntrusivePtrEnabled<Semaphore, SemaphoreDeleter>
{
public:
    Semaphore(DeviceVulkan& device, VkSemaphore semaphore, bool isSignalled);
    ~Semaphore();

    const VkSemaphore& GetSemaphore()const
    {
        return mSemaphore;
    }

    bool IsSignalled() const
    {
        return mSignalled;
    }

    uint64_t GetTimeLine()const
    {
        return mTimeline;
    }

    VkSemaphore Release()
    {
        auto semaphore = mSemaphore;
        mSemaphore = VK_NULL_HANDLE;
        mSignalled = false;
        return semaphore;
    }

    void Signal()
    {
        assert(!mSignalled);
        mSignalled = true;
    }

private:
    friend class DeviceVulkan;
    friend struct SemaphoreDeleter;
    friend class Util::ObjectPool<Semaphore>;

    DeviceVulkan& mDevice;
    VkSemaphore mSemaphore;
    bool mSignalled = true;
    uint64_t mTimeline = 0;
};
using SemaphorePtr = Util::IntrusivePtr<Semaphore>;

class SemaphoreManager
{
public:
    ~SemaphoreManager();

    void Initialize(DeviceVulkan& device);
    VkSemaphore Requset();
    void Recyle(VkSemaphore semaphore);
    void ClearAll();

private:
    DeviceVulkan* mDevice = nullptr;
    std::vector<VkSemaphore> mSeamphores;
};