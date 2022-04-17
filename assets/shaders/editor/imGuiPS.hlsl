struct VertexOutput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0;
	float4 col : COLOR0;
};

Texture2D image : register(t0);
SamplerState sam : register(s0);

float4 main(VertexOutput input) : SV_TARGET
{
	return input.col * image.Sample(sam, input.uv);
}
