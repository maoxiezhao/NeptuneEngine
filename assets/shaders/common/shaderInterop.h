#ifndef SHADER_INTEROP
#define SHADER_INTEROP

#ifdef __cplusplus 

#include "math\math.hpp"

using float4x4 = VulkanTest::FMat4x4;
using float2 = VulkanTest::F32x2;
using float3 = VulkanTest::F32x3;
using float4 = VulkanTest::F32x4;
using uint = VulkanTest::U32;
using uint2 = VulkanTest::U32x2;
using uint3 = VulkanTest::U32x3;
using uint4 = VulkanTest::U32x4;
using int2 = VulkanTest::I32x2;
using int3 = VulkanTest::I32x3;
using int4 = VulkanTest::I32x4;

#define CONSTANTBUFFER(name, type, slot)
#define PUSHCONSTANT(name, type)

#else

#define PASTE1(a, b) a##b
#define PASTE(a, b) PASTE1(a, b)
#define CONSTANTBUFFER(name, type, slot) ConstantBuffer< type > name : register(PASTE(b, slot))
#define PUSHCONSTANT(name, type) [[vk::push_constant]] type name;

#endif

// Meta macros used by shaders parser
#define META_VS(isVisible)
#define META_HS(isVisible)
#define META_DS(isVisible)
#define META_GS(isVisible)
#define META_PS(isVisible)
#define META_CS(isVisible)
#define META_PERMUTATION_1(param0)
#define META_PERMUTATION_2(param0, param1)
#define META_PERMUTATION_3(param0, param1, param2)
#define META_PERMUTATION_4(param0, param1, param2, param3)

// Common buffers:
// These are usable by all shaders
#define CBSLOT_RENDERER_FRAME					0
#define CBSLOT_RENDERER_CAMERA					1

#endif