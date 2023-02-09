#pragma once

#include "Defines.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatform.h"
#else
#error Missing Platform implementation!
#endif

#if ENABLE_ASSERTION
#define ASSERT(expression) \
    if (!(expression)) \
    { \
        if (Platform::IsDebuggerPresent()) \
        { \
            PLATFORM_DEBUG_BREAK; \
        } \
        Platform::Assert(#expression, __FILE__, __LINE__); \
    }
#else
#define ASSERT(expression) ((void)0)
#endif