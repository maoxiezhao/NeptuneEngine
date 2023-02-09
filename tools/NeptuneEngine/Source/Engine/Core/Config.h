#pragma once

#include "compiler.h"

// Test definition
#define BUILD_DEBUG 1
#define NEPTUNE_ENGINE_API
#define PLATFORM_WINDOWS 1

// Build mode
#if BUILD_DEBUG
#define BUILD_DEVELOPMENT 0
#define BUILD_RELEASE 0
#elif BUILD_DEVELOPMENT
#define BUILD_DEBUG 0
#define BUILD_RELEASE 0
#elif BUILD_RELEASE
#define BUILD_DEBUG 0
#define BUILD_DEVELOPMENT 0
#else
#error "Invalid build mode configuration"
#endif

#define LOG_ENABLE 1
#define CRASH_LOG_ENABLE (!BUILD_RELEASE)
#define ENABLE_ASSERTION 1
