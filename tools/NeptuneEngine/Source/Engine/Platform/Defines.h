#pragma once

#include "Core\Config.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsDefines.h"
#else
#error Missing Platform implementation!
#endif

#ifndef PLATFORM_DEBUG_BREAK
#define PLATFORM_DEBUG_BREAK
#endif