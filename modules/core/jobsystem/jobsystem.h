#pragma once

#include "core\common.h"

namespace VulkanTest
{
namespace Jobsystem
{
#define JOB_SYSTEM_DEBUG

    using JobHandle = U32;
    constexpr JobHandle INVALID_HANDLE = 0xFFFFFFFF;

#ifdef CJING3D_PLATFORM_WIN32
    using JobFunc = std::function<void(void*)>; //void(__stdcall *)(void*);
#else
    using JobFunc = std::function<void(void*)>;
#endif
    enum class Priority
    {
        HIGH,
        NORMAL,
        LOW,
        COUNT
    };

    constexpr U8 ANY_WORKER = 0xff;

    struct JobInfo
    {
        const char* name = "";
        JobFunc jobFunc = nullptr;
        Priority priority = Priority::NORMAL;
        void* data = nullptr;
        JobHandle precondition = INVALID_HANDLE;
        U8 workderIndex = ANY_WORKER;
    };

    bool Initialize(U32 numWorkers);
    void Uninitialize();

    void Run(const JobInfo& jobInfo, JobHandle* handle);
    void Run(void*data, JobFunc func, JobHandle* handle);
    void RunEx(void* data, JobFunc func, JobHandle* handle, JobHandle precondition);
    void Wait(JobHandle handle);

#ifdef JOB_SYSTEM_DEBUG
    void ShowDebugInfo();
#endif
}
}