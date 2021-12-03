#pragma once

#include "math_common.h"

namespace VulkanTest
{
namespace Random
{
    U32 RandomInt(U32 x);
    U32 RandomInt(U32 x, U32 y);
    F32 RandomFloat(F32 x = 1.0f);
    F32 RandomFloat(F32 x, F32 y);
    F64 RandomDouble(F64 x, F64 y);
    F64 RandomDouble(F64 x = 1.0);
}
}