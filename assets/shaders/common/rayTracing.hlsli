#ifndef RAY_TRACING_HF
#define RAY_TRACING_HF

#include "global.hlsli"

inline RayDesc CreateCameraRay(float2 clipspace)
{
	float4 worldPos = mul(GetCamera().invViewProjection, float4(clipspace, 0, 1));
	worldPos.xyz /= worldPos.w;

	RayDesc ray;
	ray.Origin = GetCamera().position;
	ray.Direction = normalize(worldPos.xyz - ray.Origin);
	ray.TMin = 0.001;
	ray.TMax = FLT_MAX;

	return ray;
}


#endif