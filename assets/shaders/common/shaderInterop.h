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

// Common buffers:
// These are usable by all shaders
#define CBSLOT_RENDERER_FRAME					0
#define CBSLOT_RENDERER_CAMERA					1

#endif