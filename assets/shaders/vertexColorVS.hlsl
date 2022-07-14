cbuffer vertexBuffer : register(b0) 
{
	float4x4	g_xTransform;
	float4		g_xColor;
};

struct Output
{
    float4 pos : SV_POSITION;
    float4 col	: COLOR;
};

Output main(float4 inPos : POSITION, float4 inCol : TEXCOORD0)
{
    Output Out;
	Out.pos = mul(g_xTransform, inPos);
	Out.col = inCol * g_xColor;
    return Out;
}