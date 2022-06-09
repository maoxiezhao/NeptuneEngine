struct VertexInput
{
	float2 pos		: POSITION;
	float2 uv		: TEXCOORD0;
	float4 col		: COLOR0;
};

struct VertexOutput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD0;
	float4 col : COLOR0;
};

cbuffer vertexBuffer : register(b0) 
{
	float4x2 ProjectionMatrix; 
};

[RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), CBV(b0), DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0))")]
VertexOutput main(VertexInput input)
{
	VertexOutput output;
	output.pos = float4(mul(float4(input.pos.xy, 1.f, 1.f), ProjectionMatrix), 0.0f, 1.0f);
	output.uv = input.uv;
	output.col = input.col; 
	return output;
}
