#include "common/global.hlsli"

PUSHCONSTANT(push, ObjectPushConstants)

inline ShaderMaterial GetMaterial()
{
    return LoadMaterial(push.materialIndex);
}

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    return GetMaterial().baseColor;
    // return float4(1.0f, 1.0f, 1.0f, 1.0f);
}