#pragma once

#include "utils\log.h"
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

template<typename T>
class Span
{
public:
	constexpr Span() noexcept = default;
	constexpr Span(T* begin, size_t len)noexcept :
		pBegin(begin),
		pEnd(begin + len)
	{}
	constexpr Span(T& begin)noexcept :
		pBegin(&begin),
		pEnd(pBegin + 1)
	{}
	constexpr Span(T* begin, T* end)noexcept :
		pBegin(begin),
		pEnd(end)
	{}
	template<size_t N>
	constexpr Span(T(&data)[N])noexcept :
		pBegin(data),
		pEnd(data + N)
	{}

	constexpr operator Span<const T>() const noexcept
	{
		return Span<const T>(pBegin, pEnd);
	}

	constexpr T& operator[](const size_t idx) const noexcept
	{
		assert(pBegin + idx < pEnd);
		return pBegin[idx];
	}

	constexpr bool empty()const noexcept {
		return length() <= 0;
	}

	constexpr size_t length()const noexcept {
		return size_t(pEnd - pBegin);
	}

	constexpr T* data()const noexcept {
		return pBegin;
	}

	// Support foreach
	constexpr T* begin()const noexcept { return pBegin; }
	constexpr T* end()const noexcept { return pEnd; }

public:
	T* pBegin = nullptr;
	T* pEnd = nullptr;
};
}