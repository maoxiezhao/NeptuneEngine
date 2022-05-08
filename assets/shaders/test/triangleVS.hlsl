struct Output
{
    float4 pos : SV_POSITION;
};

Output main(float2 pos : SV_POSITION)
{
    Output Out;
    Out.pos = float4(pos, 0.0f, 1.0f);
    return Out;
}