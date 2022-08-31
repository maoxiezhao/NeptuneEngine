#ifndef SHADER_SURFACE_HF
#define SHADER_SURFACE_HF

#include "global.hlsli"

struct Surface
{
    float3 P;				// world space position
	float3 N;				// world space normal
	float3 V;				// world space view vector
	float4 T;				// tangent

    float4 baseColor;

    inline void Init()
    {
		P = 0;
		V = 0;
		N = 0;
        T = 0;

        baseColor = 1;
    }
};

#endif