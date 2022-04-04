#pragma once

#include "math_common.h"

#ifdef _MSC_VER

#include <intrin.h>
#if defined(_INCLUDED_PMM) && !defined(__SSE3__)
#define __SSE3__ 1
#endif
#if !defined(__SSE__)
#define __SSE__ 1
#endif
#if defined(_INCLUDED_IMM) && !defined(__AVX__)
#define __AVX__ 1
#endif

#elif defined(__AVX__)
#include <immintrin.h>
#elif defined(__SSE3__)
#include <pmmintrin.h>
#elif defined(__SSE__)
#include <xmmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace VulkanTest
{
#ifdef CJING3D_PLATFORM_WIN32
	using FVector = __m128;

	CJING_FORCE_INLINE FVector FVLoadUnaligned(const void* src)
	{
		return _mm_loadu_ps((const F32*)(src));
	}

	CJING_FORCE_INLINE FVector FVLoad(const void* src)
	{
		return _mm_load_ps((const F32*)(src));
	}

	CJING_FORCE_INLINE FVector FVSplat(F32 value)
	{
		return _mm_set_ps1(value);
	}

	CJING_FORCE_INLINE F32 FVGetX(FVector v)
	{
		return _mm_cvtss_f32(v);
	}

	CJING_FORCE_INLINE F32 FVGetY(FVector v)
	{
		FVector r = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 1));
		return _mm_cvtss_f32(r);
	}

	CJING_FORCE_INLINE F32 FVGetZ(FVector v)
	{
		FVector r = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 2));
		return _mm_cvtss_f32(r);
	}

	CJING_FORCE_INLINE F32 FVGetW(FVector v)
	{
		FVector r = _mm_shuffle_ps(v, v, _MM_SHUFFLE(0, 0, 0, 3));
		return _mm_cvtss_f32(r);
	}
#else
	struct FVector
	{
		F32 x, y, z, w;
	};
#endif
}
