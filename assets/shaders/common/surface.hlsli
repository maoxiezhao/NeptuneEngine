#ifndef SHADER_SURFACE_HF
#define SHADER_SURFACE_HF

#include "global.hlsli"

// Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
float3 FresnelSchlick(float3 f0, float f90, float VdotH)
{
    return f0 + (f90 - f0) * pow(1.0 - VdotH, 5);
}

struct Surface
{
    float3 P;				// world space position
	float3 N;				// world space normal
	float3 V;				// world space view vector
	float4 T;				// tangent

    float4 baseColor;       // base color
    float3 albedo;          // diffuse light absorbtion value
    float roughness;        // perceptual roughness
    float3 f0;

    float roughnessBRDF;	// real roughness (perceptualRoughness->roughness)
	float f90;				// reflectance at grazing angle
    float NdotV;

    // Init default surface
    inline void Init()
    {
		P = 0;
		V = 0;
		N = 0;
        T = 0;

        baseColor = 1;
        albedo = 1;
        roughness = 1;
        f0 = 0;
    }

    // Setup surface
    inline void Setup(in ShaderMaterial material, in float4 baseColor_)
    {
        baseColor = baseColor_;
        roughness = material.roughness;

        const float metalness = material.metalness;
        const float reflectance = material.reflectance;
        albedo = baseColor.rgb * (1 - max(reflectance, metalness));
        f0 = lerp(reflectance.xxx, baseColor.rgb, metalness);
    }

    inline void Update()
    {
        float perceptualRoughness = clamp(roughness, 0.045, 1);
		roughnessBRDF = perceptualRoughness * perceptualRoughness;   
        f90 = saturate(50.0 * dot(f0, 0.33));

        NdotV = saturate(dot(N, V) + 1e-5);
    }
};

#endif