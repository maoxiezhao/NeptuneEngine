#ifndef SHADER_GLOBAL_HF
#define SHADER_GLOBAL_HF

#include "shaderInterop.h"
#include "shaderInterop_renderer.h"

// Common definitions
#define PI 3.14159265358979323846
#define FLT_MAX 3.402823466e+38

// Bindless resources
ByteAddressBuffer bindless_buffers[] : register(space1);
Texture2D bindless_textures[] : register(space2);
Buffer<uint> bindless_ib[] : register(space3);

// Immutable samplers
SamplerState samLinearClamp : register(s100);
SamplerState samLinearWrap  : register(s101);
SamplerState samPointClamp  : register(s102);
SamplerState samPointWrap   : register(s103);

// Bindless textures
#define texture_depth bindless_textures[GetCamera().texture_depth_index]

/////////////////////////////////////////////////////////////////////////

inline FrameCB GetFrame()
{
	return g_xFrame;
}

inline ShaderSceneCB GetScene()
{
	return g_xFrame.scene;
}

inline CameraCB GetCamera()
{
	return g_xCamera;
}

inline ShaderGeometry LoadGeometry(uint geometryIndex)
{
	return bindless_buffers[GetScene().geometrybuffer].Load<ShaderGeometry>(geometryIndex * sizeof(ShaderGeometry));
}

inline ShaderMaterial LoadMaterial(uint materialIndex)
{
	return bindless_buffers[GetScene().materialbuffer].Load<ShaderMaterial>(materialIndex * sizeof(ShaderMaterial));
}

inline ShaderMeshInstance LoadInstance(uint instanceIndex)
{
	return bindless_buffers[GetScene().instancebuffer].Load<ShaderMeshInstance>(instanceIndex * sizeof(ShaderMeshInstance));
}

inline ShaderMeshlet LoadMeshlet(uint meshletIndex)
{
	return bindless_buffers[GetScene().meshletBuffer].Load<ShaderMeshlet>(meshletIndex * sizeof(ShaderMeshlet));
}

inline ShaderLight GetShaderLight(uint index)
{
	return bindless_buffers[GetFrame().bufferShaderLightsIndex].Load<ShaderLight>(index * sizeof(ShaderLight));
}

/////////////////////////////////////////////////////////////////////////

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 ApplySRGBCurveFast( float3 x )
{
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 RemoveSRGBCurveFast( float3 x )
{
    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

#define DEGAMMA(x)		(RemoveSRGBCurveFast(x))
#define GAMMA(x)		(ApplySRGBCurveFast(x))

inline float4 unpack_rgba(in uint value)
{
	float4 retVal;
	retVal.x = (float)((value >> 0u) & 0xFF) / 255.0;
	retVal.y = (float)((value >> 8u) & 0xFF) / 255.0;
	retVal.z = (float)((value >> 16u) & 0xFF) / 255.0;
	retVal.w = (float)((value >> 24u) & 0xFF) / 255.0;
	return retVal;
}

inline uint flatten2D(uint2 coord, uint2 dim)
{
	return coord.x + coord.y * dim.x;
}

inline uint2 unflatten2D(uint idx, uint2 dim)
{
	return uint2(idx % dim.x, idx / dim.x);
}

float ComputeLinearDepth(in float z, in float near, in float far)
{
	float z_n = 2 * z - 1;
	return 2 * far * near / (near + far - z_n * (near - far));
}

float ComputeLinearDepth(in float z)
{
	return ComputeLinearDepth(z, GetCamera().zNear, GetCamera().zFar);
}

// [0, 1] => [-1, 1] and Flip y
inline float2 UVtoClipspace(in float2 uv)
{
	float2 clipspace = uv * 2 - 1;
	clipspace.y *= -1;
	return clipspace;
}

// Compute barycentric coordinates on triangle from a ray
// Moller-Trumbore with cramer'rules
float2 ComputeBarycentrics(float3 rayOrigin, float3 rayDirection, float3 a, float3 b, float3 c)
{
	float3 v0v1 = b - a;
	float3 v0v2 = c - a;
	float3 pvec = cross(rayDirection, v0v2);
	float det = dot(v0v1, pvec);
	float det_rcp = rcp(det);
	float3 tvec = rayOrigin - a;
	float u = dot(tvec, pvec) * det_rcp;
	float3 qvec = cross(tvec, v0v1);
	float v = dot(rayDirection, qvec) * det_rcp;
	return float2(u, v);
}

// Compute barycentric coordinates on triangle from a ray
// Moller-Trumbore with cramer'rules
float2 ComputeBarycentrics(float3 rayOrigin, float3 rayDirection, float3 a, float3 b, float3 c, out float t)
{
	float3 v0v1 = b - a;
	float3 v0v2 = c - a;
	float3 pvec = cross(rayDirection, v0v2);
	float det = dot(v0v1, pvec);
	float det_rcp = rcp(det);
	float3 tvec = rayOrigin - a;
	float u = dot(tvec, pvec) * det_rcp;
	float3 qvec = cross(tvec, v0v1);
	float v = dot(rayDirection, qvec) * det_rcp;
	t = dot(v0v2, qvec) * det_rcp;
	return float2(u, v);
}

#endif