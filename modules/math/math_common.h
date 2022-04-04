#pragma once

#include <math.h>

namespace VulkanTest
{
using U8 = unsigned __int8;
using U16 = unsigned __int16;
using U32 = unsigned __int32;
using U64 = unsigned __int64;

using F32 = float;
using F64 = double;

using I8 = __int8;
using I16 = __int16;
using I32 = __int32;
using I64 = __int64;

using F32 = float;
using F64 = double;

#ifdef _WIN32
#define CJING_FORCE_INLINE __forceinline
#endif

// Use directXMath temporarily
#define USE_DIRECTX_MATH
}