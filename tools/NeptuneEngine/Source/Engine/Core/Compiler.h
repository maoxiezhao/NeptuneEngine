#pragma once

#if defined(_MSC_VER)

#pragma warning( disable :26812)
#pragma warning( disable :4251)

#define COMPILER_MSVC 1
#define NOEXCEPT

#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)
#define FORCE_INLINE __forceinline
#define RESTRICT __restrict
#define INLINE __inline
#define PACK_BEGIN() __pragma(pack(push, 1))
#define PACK_END() ; __pragma(pack(pop))
#define ALIGN_BEGIN(_align) __declspec(align(_align))
#define ALIGN_END(_align)
#define OFFSET_OF(X, Y) offsetof(X, Y)
#define DEPRECATED __declspec(deprecated)

#else 
#error "Only support MSV now."
#endif