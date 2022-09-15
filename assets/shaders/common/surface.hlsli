#ifndef SHADER_SURFACE_HF
#define SHADER_SURFACE_HF

#include "global.hlsli"
#include "primitive.hlsli"

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
    float2 Pixel;           // pixel coordinate

    float4 baseColor;       // base color
    float3 albedo;          // diffuse light absorbtion value
    float roughness;        // perceptual roughness
    float3 f0;

    float roughnessBRDF;	// real roughness (perceptualRoughness->roughness)
	float f90;				// reflectance at grazing angle
    float3 F;                // fresnel
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

    void Update()
    {
        float perceptualRoughness = clamp(roughness, 0.045, 1);
		roughnessBRDF = perceptualRoughness * perceptualRoughness;   
        f90 = saturate(50.0 * dot(f0, 0.33));
        NdotV = saturate(dot(N, V) + 1e-5);
        F = FresnelSchlick(f0, f90, NdotV);
    }

    // Use promitive to reconstruction the surface
    ShaderMeshInstance inst;
	ShaderGeometry geometry;
    // Indices
    uint i0;
	uint i1;
	uint i2;
    // Positions
    float3 pos0;
    float3 pos1;
    float3 pos2;

    bool LoadFromPrimitive(in PrimitiveID prim, in float3 rayOrigin, in float3 rayDirection)
    {
        ShaderMeshInstance inst = LoadInstance(prim.instanceIndex);
        ShaderGeometry geometry = LoadGeometry(inst.geometryOffset + prim.subsetIndex);

        const uint startIndex = prim.primitiveIndex * 3 + geometry.indexOffset;
        Buffer<uint> indexBuffer = bindless_ib[NonUniformResourceIndex(geometry.ib)];
        i0 = indexBuffer[startIndex + 0];
        i1 = indexBuffer[startIndex + 1];
        i2 = indexBuffer[startIndex + 2];

        ByteAddressBuffer posBuffer = bindless_buffers[NonUniformResourceIndex(geometry.vbPosNor)];
        uint4 data0 = posBuffer.Load4(i0 * sizeof(uint4));
        uint4 data1 = posBuffer.Load4(i1 * sizeof(uint4));
        uint4 data2 = posBuffer.Load4(i2 * sizeof(uint4));
        float3 p0 = asfloat(data0.xyz);
        float3 p1 = asfloat(data1.xyz);
        float3 p2 = asfloat(data2.xyz);
        pos0 = mul(inst.transform.GetMatrix(), float4(p0, 1)).xyz;
        pos1 = mul(inst.transform.GetMatrix(), float4(p1, 1)).xyz;
        pos2 = mul(inst.transform.GetMatrix(), float4(p2, 1)).xyz;

        float hitDepth;
        ComputeBarycentrics(rayOrigin, rayDirection, pos0, pos1, pos2, hitDepth);
        P = rayOrigin + rayDirection * hitDepth;
        V = -rayDirection;

        return true;
    }
};

#endif