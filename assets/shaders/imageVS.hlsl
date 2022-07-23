#include "common/shaderInterop_image.h"

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
    [branch]
	if (image.flags & IMAGE_FLAG_FULLSCREEN)
	{
        FullScreenTriangle(vID, Out.pos, Out.uv);
    }
    else
    {
        //	1--2
		//	  /
		//	 /
		//	3--4
        switch (vID)
		{
		default:
		case 0:
			Out.pos = image.corners0;
            Out.uv = float2(0, 0);
			break;
		case 1:
			Out.pos = image.corners1;
            Out.uv = float2(1, 0);
			break;
		case 2:
			Out.pos = image.corners2;
            Out.uv = float2(0, 1);
			break;
		case 3:
			Out.pos = image.corners3;
            Out.uv = float2(1, 1);
			break;
		}
    }
    return Out;
}