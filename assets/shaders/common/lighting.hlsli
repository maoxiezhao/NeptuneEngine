#ifndef LIGHTING_HF
#define LIGHTING_HF

#include "global.hlsli"
#include "surface.hlsli"
#include "brdf.hlsli"

uint LoadEntityTile(uint tileIndex)
{
    return bindless_buffers[GetCamera().cullingTileBufferIndex].Load(tileIndex * sizeof(uint));
}

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

inline void ForwardLighting(inout Surface surface, inout Lighting lighting, uint tileBucketStart)
{
    [branch]
    if (GetFrame().lightCount > 0)
    {
        // Divide all lights into a bucket of 32
        const uint firstLight = GetFrame().lightOffset;
        const uint lastLight = GetFrame().lightOffset + GetFrame().lightCount - 1;
        const uint firstBucket = firstLight / 32;
        const uint lastBucket = min(lastLight / 32, max(0, SHADER_ENTITY_TILE_BUCKET_COUNT - 1));
        
        [loop]
        for (int bucket = firstBucket; bucket <= lastBucket; bucket++)
        {
            uint lightsBits = LoadEntityTile(tileBucketStart + bucket);

            // Bucket scalarizer - Siggraph 2017 - Improved Culling [Michal Drobot]:
			lightsBits = WaveReadLaneFirst(WaveActiveBitOr(lightsBits));

            [loop]
            while(lightsBits != 0)
            {
                const uint lightBitIndex = firstbitlow(lightsBits);
                const uint lightIndex = bucket * 32 + lightBitIndex;
                lightsBits ^= 1u << lightBitIndex;

                lighting.direct.diffuse.r = lightIndex;

                [branch]
                if (lightIndex >= firstLight && lightIndex <= lastLight)
                {
                    ShaderLight light = GetShaderLight(lightIndex);
                    switch(light.GetType())
                    {
                    case SHADER_LIGHT_TYPE_DIRECTIONALLIGHT:
                        LightPoint(light, surface, lighting);
                        break;
                    case SHADER_LIGHT_TYPE_POINTLIGHT:
                        LightPoint(light, surface, lighting);
                        break;
                    case SHADER_LIGHT_TYPE_SPOTLIGHT:
                        LightPoint(light, surface, lighting);
                        break;
                    }
                }
                else if (lightIndex >= lastLight)
                {
                    bucket = SHADER_ENTITY_TILE_BUCKET_COUNT;
                    break;
                }
            }
        }
    }
}

inline void TiledForwardLighting(inout Surface surface, inout Lighting lighting)
{
    const uint2 tileIndex = uint2(floor(surface.Pixel / TILED_CULLING_BLOCK_SIZE));
    const uint flatTileIndex = flatten2D(tileIndex, GetCamera().cullingTileCount.xy);
    ForwardLighting(surface, lighting, flatTileIndex * SHADER_ENTITY_TILE_BUCKET_COUNT);
}

#endif