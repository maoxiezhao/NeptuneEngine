#pragma once

#include "version.h"
#include "platform\defines.h"
#include "utils\log.h"
#include "types\pair.h"
#include "types\span.h"
#include "math\math.hpp"

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <assert.h>
#include <math.h>
#include <functional>

#ifdef _MSC_VER
#pragma warning( disable :26812)
#pragma warning( disable :4819 4996 6031)
#define COMPILER_MSVC 1
#define NOEXCEPT
#else
#define COMPILER_MSVC 0
#endif

#ifdef _WIN32
#define LIBRARY_EXPORT __declspec(dllexport)
#define LIBRARY_IMPORT __declspec(dllimport)
#define FORCE_INLINE __forceinline
#define RESTRICT __restrict
#else 
#define LIBRARY_EXPORT __attribute__((visibility("default")))
#define FORCE_INLINE 
#define FORCE_INLINE __attribute__((always_inline)) inline
#define RESTRICT __restrict__
#endif

#ifdef STATIC_PLUGINS
#define VULKAN_TEST_API
#elif defined BUILDING_ENGINE
#define VULKAN_TEST_API LIBRARY_EXPORT
#else
#define VULKAN_TEST_API LIBRARY_IMPORT
#endif

#ifndef PLATFORM_THREADS_LIMIT
#define PLATFORM_THREADS_LIMIT 64
#endif

// Pointer as integer and pointer size
#ifdef PLATFORM_64BITS
typedef VulkanTest::U64 uintptr;
typedef VulkanTest::I64 intptr;
#define POINTER_SIZE 8
#else
typedef VulkanTest::U32 uintptr;
typedef VulkanTest::I32 intptr;
#define POINTER_SIZE 4
#endif

#ifdef DEBUG
#define ASSERT(x)                                                \
	do                                                           \
	{                                                            \
		if (!bool(x))                                            \
		{                                                        \
			Logger::Error("Error at %s:%d.\n", __FILE__, __LINE__); \
			abort();                                             \
		}                                                        \
	} while (0)

#define ASSERT_MSG(x, msg)                                       \
	do                                                           \
	{                                                            \
		if (!bool(x))                                            \
		{                                                        \
			Logger::Error("Error at %s:%d. %s\n", __FILE__, __LINE__, msg); \
			abort();                                             \
		}                                                        \
	} while (0)
#else
#define ASSERT(x) ((void)0)
#define ASSERT(x, msg) ((void)0)
#endif

namespace VulkanTest
{

static const int MAX_PATH_LENGTH = 256;

using UIntPtr = U64;
static_assert(sizeof(UIntPtr) == sizeof(void*), "Incorrect size of uintptr");
static_assert(sizeof(I64) == 8, "Incorrect size of i64");
static_assert(sizeof(I32) == 4, "Incorrect size of i32");
static_assert(sizeof(I16) == 2, "Incorrect size of i16");
static_assert(sizeof(I8) == 1, "Incorrect size of i8");


template<typename ENUM>
constexpr inline bool FLAG_ALL(ENUM value, ENUM Flags)
{
	static_assert(sizeof(ENUM) <= sizeof(int), "Enum size too large.");
	return ((int)value & (int)Flags) == (int)Flags;
}

constexpr inline bool FLAG_ALL(int value, int Flags) {
	return ((int)value & (int)Flags) == (int)Flags;
}

template<typename ENUM>
constexpr inline bool FLAG_ANY(ENUM value, ENUM Flags)
{
	static_assert(sizeof(ENUM) <= sizeof(int), "Enum size too large.");
	return ((int)value & (int)Flags) != 0;
}

constexpr inline bool FLAG_ANY(int value, int Flags) {
	return ((int)value & (int)Flags) != 0;
}

template <typename T, U32 count> 
constexpr U32 LengthOf(const T(&)[count])
{
	return count;
};

#ifdef __GNUC__

#elif defined(_MSC_VER)
// Find first one bit (MSB to LSB)
static inline uint32_t LeadingZeroes(uint32_t x)
{
	unsigned long result;
	if (_BitScanReverse(&result, x))
		return 31 - result;
	else
		return 32;
}

// Find first one bit (LSB to MSB)
static inline uint32_t TrailingZeroes(uint32_t x)
{
	unsigned long result;
	if (_BitScanForward(&result, x))
		return result;
	else
		return 32;
}

// Find first zero bit (LSB to MSB)
static inline uint32_t TrailingOnes(uint32_t x)
{
	return TrailingZeroes(~x);
}
#else
#error "Implement me."
#endif

template <typename T>
inline void ForEachBit(uint32_t value, const T & func)
{
	while (value)
	{
		uint32_t bit = TrailingZeroes(value);
		func(bit);
		value &= ~(1u << bit);
	}
}

template <typename T>
inline void ForEachBitRange(uint32_t value, const T& func)
{
	if (value == ~0u)
	{
		func(0, 32);
		return;
	}

	U32 onesRange = 0;
	U32 bitOffset = 0;
	while (value)
	{
		uint32_t bit = TrailingZeroes(value);
		bitOffset += bit;
		value >>= bit;

		// Find ones range
		onesRange = TrailingOnes(value);
		func(bitOffset, onesRange);
		value &= ~((1u << onesRange) - 1);
	}
}

constexpr U32 AlignTo(U32 value, U32 alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
}

constexpr U64 AlignTo(U64 value, U64 alignment)
{
	return ((value + alignment - 1) / alignment) * alignment;
}

}