
struct Output
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};
inline void FullScreenTriangle(in uint vertexID, out float4 pos, out float2 uv)
{
	pos.x = (float)(vertexID / 2) * 4.0 - 1.0;
	pos.y = (float)(vertexID % 2) * 4.0 - 1.0;
	pos.z = 0;
	pos.w = 1;
    uv.x = (float)(vertexID / 2) * 2;
	uv.y = 1 - (float)(vertexID % 2) * 2;
}

Output main(uint vID : SV_VERTEXID)
{
    Output Out;
    FullScreenTriangle(vID, Out.pos, Out.uv);
    return Out;
}