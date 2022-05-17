#include "common/global.hlsli"

struct Output
{
    float4 pos : SV_POSITION;
};

Output main(float3 pos : SV_POSITION)
{
    Output Out;
    Out.pos = float4(pos, 1.0f);
    Out.pos = mul(GetCamera().viewProjection, Out.pos);
    return Out;
}