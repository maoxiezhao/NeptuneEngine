#pragma once

#include "utils\log.h"

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <assert.h>
#include <math.h>
#include <functional>

#ifdef _MSC_VER
#pragma warning( disable :26812)
#pragma warning( disable :4819 4996)
#define COMPILER_MSVC 1
#define NOEXCEPT
#else
#define COMPILER_MSVC 0
#endif

using U8 = unsigned __int8;
using U16 = unsigned __int16;
using U32 = unsigned __int32;
using U64 = unsigned __int64;

using I8 = __int8;
using I16 = __int16;
using I32 = __int32;
using I64 = __int64;

using F32 = float;
using F64 = double;

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
#else
#define ASSERT(x) ((void)0)
#endif

static inline uint32_t trailingZeroes(uint32_t x)
{
	unsigned long result;
	if (_BitScanForward(&result, x))
		return result;
	else
		return 32;
}

template <typename T>
inline void ForEachBit(uint32_t value, const T & func)
{
	while (value)
	{
		uint32_t bit = trailingZeroes(value);
		func(bit);
		value &= ~(1u << bit);
	}
}