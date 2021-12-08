
struct Output
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

Output main(float2 pos : SV_POSITION, float2 uv : TEXCOORD0)
{
    Output Out;
    Out.pos = float4(pos, 0.0f, 1.0f);
    Out.uv = uv;
    return Out;
}