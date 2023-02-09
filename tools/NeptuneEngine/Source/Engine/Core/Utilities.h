#pragma once

#include "types\baseTypes.h"
#if _MSC_VER
#include <intrin.h>
#endif

namespace Utilities
{
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

#if defined(_MSC_VER)
	// Find first one bit (MSB to LSB)
	static inline U32 LeadingZeroes(U32 x)
	{
		unsigned long result;
		if (_BitScanReverse(&result, x))
			return 31 - result;
		else
			return 32;
	}

	// Find first one bit (LSB to MSB)
	static inline U32 TrailingZeroes(U32 x)
	{
		unsigned long result;
		if (_BitScanForward(&result, x))
			return result;
		else
			return 32;
	}

	// Find first zero bit (LSB to MSB)
	static inline U32 TrailingOnes(U32 x)
	{
		return TrailingZeroes(~x);
	}
#else
#error "Implement me."
#endif

	template <typename T>
	inline void ForEachBit(U32 value, const T& func)
	{
		while (value)
		{
			U32 bit = TrailingZeroes(value);
			func(bit);
			value &= ~(1u << bit);
		}
	}

	template <typename T>
	inline void ForEachBitRange(U32 value, const T& func)
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
			U32 bit = TrailingZeroes(value);
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
