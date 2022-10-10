#pragma once

#if CJING3D_PLATFORM_WIN32

// Platform description
#define PLATFORM_DESKTOP 1
#if defined(WIN64)
#define PLATFORM_64BITS 1
#define PLATFORM_ARCH_X64 1
#define PLATFORM_ARCH ArchitectureType::x64
#else
#define PLATFORM_64BITS 0
#define PLATFORM_ARCH_X86 1
#define PLATFORM_ARCH ArchitectureType::x86
#endif
#define PLATFORM_CACHE_LINE_SIZE 128
#define PLATFORM_LINE_TERMINATOR "\r\n"
#define PLATFORM_DEBUG_BREAK __debugbreak()

// Unicode text macro
#if !defined(TEXT)
#if PLATFORM_TEXT_IS_CHAR16
#define _TEXT(x) u ## x
#else
#define _TEXT(x) L ## x
#endif
#define TEXT(x) _TEXT(x)
#endif


#endif
