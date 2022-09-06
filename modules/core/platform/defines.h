#pragma once

#ifdef CJING3D_PLATFORM_WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#if !defined(NOMINMAX) && defined(_MSC_VER)
#define NOMINMAX
#endif
#endif

#if CJING3D_PLATFORM_WIN32
#include "win32\defines.h"
#else
#error Missing Defines implementation!
#endif