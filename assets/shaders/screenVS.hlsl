
struct Output
{
    float4 pos : SV_POSITION;
};

inline void FullScreenTriangle(in uint vertexID, out float4 pos)
{
	pos.x = (float)(vertexID / 2) * 4.0 - 1.0;
	pos.y = (float)(vertexID % 2) * 4.0 - 1.0;
	pos.z = 1;
	pos.w = 1;
}

Output main(uint vID : SV_VERTEXID)
{
    Output Out;
    FullScreenTriangle(vID, Out.pos);
    return Out;
}