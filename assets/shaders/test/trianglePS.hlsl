
Texture2D bindless_textures[] : register(space1);
SamplerState sam : register(s0);

struct Input
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(Input input) : SV_TARGET
{
    return float4(input.uv, 1.0f, 1.0f);
}