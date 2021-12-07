
struct Output
{
    float4 pos : SV_POSITION;
    // float4 col : COLOR0;
};

Output main(float2 pos : SV_POSITION)
{
    Output Out;
    Out.pos = float4(pos, 0.0f, 1.0f);
    // Out.col = float4(1.0f, 1.0f, 1.0f, 1.0f);
    return Out;
}