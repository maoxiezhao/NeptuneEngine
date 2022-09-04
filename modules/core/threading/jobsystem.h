#pragma once

#include "core\common.h"

namespace VulkanTest
{
namespace Jobsystem
{
    constexpr U8 ANY_WORKER = 0xff;

    using JobFunc = std::function<void(void*)>;

    struct JobHandle
    {
        ~JobHandle() 
        {
            ASSERT(waitor == nullptr);
            ASSERT(counter == 0);
        }

        explicit operator bool()const {
            return counter > 0;
        }

        volatile I32 counter = 0;
        struct JobWaitor* waitor = nullptr;
        I32 generation = 0;
    };

    bool Initialize(U32 numWorkers);
    void Uninitialize();

    void Run(void*data, JobFunc func, JobHandle* handle, U8 workerIndex = ANY_WORKER);
    void Wait(JobHandle* handle);
}
}