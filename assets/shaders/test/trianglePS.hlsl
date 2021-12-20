
Texture2D bindless_textures[] : register(space1);
SamplerState sam : register(s0);

struct PushConstantImage
{
    int textureIndex;
};
[[vk::push_constant]] PushConstantImage push;

struct Input
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(Input input) : SV_TARGET
{
    int index = (int)(floor(input.uv.x / 0.2f) + floor(input.uv.y / 0.2f) ) % 4;
    return bindless_textures[index].Sample(sam, float2(0.5f, 0.5f));
}