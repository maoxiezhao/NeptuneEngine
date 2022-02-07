#pragma once

#include "core\common.h"

namespace VulkanTest
{
namespace Jobsystem
{
    using JobHandle = U32;
    using JobFunc = std::function<void(void*)>;

    constexpr JobHandle INVALID_HANDLE = 0xFFFFFFFF;
    constexpr U8 ANY_WORKER = 0xff;

    bool Initialize(U32 numWorkers);
    void Uninitialize();

    void Run(void*data, JobFunc func, JobHandle* handle, U8 workerIndex = ANY_WORKER);
    void Wait(JobHandle handle);
}
}