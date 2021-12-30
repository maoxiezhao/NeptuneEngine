#pragma once

#include "core\common.h"

// Fiber is only available in win32
namespace VulkanTest
{
namespace Fiber
{

#ifdef CJING3D_PLATFORM_WIN32
    using Handle = void*;
    constexpr Handle INVALID_HANDLE = nullptr;
    using JobFunc = void(__stdcall *)(void*);
#else
    using Handle = U32;
    constexpr Handle INVALID_HANDLE = 0xFFFFFFFF;
    using JobFunc = void (*)(void*);
#endif

    enum ThisThread
    {
        THIS_THREAD
    };
    
    Handle Create(ThisThread);
    Handle Create(int stackSize, JobFunc proc, void* parameter);
    void Destroy(Handle fiber);
    void SwitchTo(Handle from, Handle to);
    bool IsValid(Handle fiber);
}
}