#ifndef LIGHTING_HF
#define LIGHTING_HF

#include "global.hlsli"
#include "surface.hlsli"
#include "brdf.hlsli"

struct LightingPart
{
    float3 diffuse;
    float3 specular;
};

struct Lighting
{
    LightingPart direct;

    void Init(float3 directDiffuse, float3 directSpecular)
    {
        direct.diffuse = directDiffuse;
        direct.specular = directSpecular;
    }
};

// Apply lighting to color (Combine direct lighting and indirect lighting)
inline void ApplyLighting(in Surface surface, in Lighting lighting, inout float4 color)
{
    float3 diffuse = lighting.direct.diffuse / PI;
    float3 specular = lighting.direct.specular;
    color.rgb = surface.albedo * diffuse + specular;
}

inline float CalculatePointLightAttenuation(in float dist, in float dist2, in float range, in float range2)
{
	// GLTF recommendation: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#range-property
	//return saturate(1 - pow(dist / range, 4)) / dist2;
	float distInvRange2 = dist2 / max(0.0001, range2);
	distInvRange2 *= distInvRange2;           // pow4
    return saturate(1 - distInvRange2) / max(0.0001, dist2);
}

// Point light lighting
inline void LightPoint(in ShaderLight light, in Surface surface, inout Lighting lighting)
{
    float3 l = light.position - surface.P;
    const float dist2 = dot(l, l);
    const float range = light.GetRange();
    const float range2 = range * range;

    [branch]
    if (dist2 < range2)
    {
        float dist = sqrt(dist2);
        l /= dist;

        // Create light to surface
        LightSurface lightSurface;
        lightSurface.Create(surface, l);
        [branch]
        if (any(lightSurface.NdotL))
        {
            float attenuation = CalculatePointLightAttenuation(dist, dist2, range, range2);
            float3 lightColor = light.GetColor().rgb * attenuation;

            lighting.direct.diffuse = mad(lightColor, BRDF_GetDiffuse(surface, lightSurface), lighting.direct.diffuse);
            lighting.direct.specular = mad(lightColor, BRDF_GetSpecular(surface, lightSurface), lighting.direct.specular);
        }
    }
}

inline void ForwardLighting(inout Surface surface, inout Lighting lighting)
{
    [branch]
    if (GetFrame().lightCount > 0)
    {
        ShaderLight light = GetShaderLight(0);
        switch(light.GetType())
        {
        case SHADER_LIGHT_TYPE_POINTLIGHT:
            LightPoint(light, surface, lighting);
            break;
        }
    }
}

#endif