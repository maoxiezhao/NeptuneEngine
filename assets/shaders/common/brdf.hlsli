#ifndef BRDF_HF
#define BRDF_HF

#include "global.hlsli"
#include "surface.hlsli"

#define MEDIUMP_FLT_MAX    65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#define highp

// BRDF functions source:
// https://github.com/google/filament/blob/main/shaders/src/brdf.fs

float D_GGX(float roughness, float NdotH)
{
	// Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
	float oneMinusNoHSquared = 1.0 - NdotH * NdotH;

	float a = NdotH * roughness;
	float k = roughness / (oneMinusNoHSquared + a * a);
	float d = k * k * (1.0 / PI);
	return saturateMediump(d);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL)
{
	// Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
	float a2 = roughness * roughness;
	// TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
	float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
	float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
	float v = 0.5 / (lambdaV + lambdaL);
	// a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
	// a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
	// clamp to the maximum value representable in mediump
	return saturateMediump(v);
}

// Light to surface
struct LightSurface
{
    float3 L;    // Surface to light
    float3 H;    // Half vector between V and L
    float3 F;     // fresnel term
    float NdotL; 
    float VdotH;
    float NdotH;

    inline void Create(in Surface surface, in float3 l)
    {
        L = l;
        H = normalize(L + surface.V);
        NdotL = saturate(dot(L, surface.N));
        VdotH = saturate(dot(surface.V, H));
        NdotH = saturate(dot(surface.N, H));
        F = FresnelSchlick(surface.f0, surface.f90, VdotH);
    }
};

// Kd = (1 - ks(Fresnel) * (N * L)
float3 BRDF_GetDiffuse(in Surface surface, in LightSurface lightSurface)
{
    return (1.0f - lightSurface.F) * lightSurface.NdotL;
}

// ks = (D * V) * F;
float3 BRDF_GetSpecular(in Surface surface, in LightSurface lightSurface)
{
    float D = D_GGX(surface.roughnessBRDF, lightSurface.NdotH);
    float V = V_SmithGGXCorrelated(surface.roughnessBRDF, surface.NdotV, lightSurface.NdotL);
    float3 specular = D * V * lightSurface.F;
    return specular * lightSurface.NdotL;
}

#endif