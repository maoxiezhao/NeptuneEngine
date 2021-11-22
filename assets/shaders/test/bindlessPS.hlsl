
Texture2D bindless_textures[] : register(space1);
Texture2D bindless_textures2[] : register(space2);
SamplerState sam : register(s0);

float4 main(float4 pos : SV_POSITION) : SV_TARGET
{
    int x = int(pos.x / 16.0) & 1023;
    int y = int(pos.y / 16.0) & 1023;
    int index = x ^ y;

    return bindless_textures[index].Sample(sam, float2(0.5f, 0.5f)) *
        bindless_textures2[index].Sample(sam, float2(0.5f, 0.5f));
}