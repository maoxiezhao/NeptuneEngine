#pragma once

#include "math_common.h"

//#ifdef _MSC_VER
//
//#include <intrin.h>
//#if defined(_INCLUDED_PMM) && !defined(__SSE3__)
//#define __SSE3__ 1
//#endif
//#if !defined(__SSE__)
//#define __SSE__ 1
//#endif
//#if defined(_INCLUDED_IMM) && !defined(__AVX__)
//#define __AVX__ 1
//#endif
//
//#elif defined(__AVX__)
//#include <immintrin.h>
//#elif defined(__SSE3__)
//#include <pmmintrin.h>
//#elif defined(__SSE__)
//#include <xmmintrin.h>
//#elif defined(__ARM_NEON)
//#include <arm_neon.h>
//#endif