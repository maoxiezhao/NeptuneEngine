#include "common/objectHF.hlsli"

#ifdef OBJECTSHADER_LAYOUT_PREPASS
[earlydepthstencil]
uint main(PixelInput input) : SV_Target
{
    return 0;
}
#else

float4 main(PixelInput input) : SV_Target
{
    return GetMaterial().baseColor;
}
#endif