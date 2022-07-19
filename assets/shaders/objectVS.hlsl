#include "common/objectHF.hlsli"

PixelInput main(VertexInput input)
{
    VertexSurface surface;
    surface.Create(input);

    PixelInput Out;
    Out.pos = surface.position;
    Out.pos = mul(GetCamera().viewProjection, Out.pos);

#ifdef OBJECTSHADER_USE_COLOR
    Out.color = surface.color;
#endif

#ifdef OBJECTSHADER_USE_NORMAL
    Out.nor = surface.normal;
#endif

#ifdef OBJECTSHADER_USE_POSITION3D
    Out.pos3D = surface.position.xyz;
#endif

#ifdef OBJECTSHADER_USE_INSTANCEINDEX
	Out.instanceIndex = input.GetInstancePointer().instanceIndex;
#endif

    return Out;
}