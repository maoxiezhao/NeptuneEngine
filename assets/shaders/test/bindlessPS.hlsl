
Texture2D bindless_textures[] : register(space1);
SamplerState sam : register(s0);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    return bindless_textures[0].Sample(sam, float2(0.0f, 0.0f));
}