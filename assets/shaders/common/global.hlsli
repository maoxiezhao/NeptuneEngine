#ifndef SHADER_GLOBAL_HF
#define SHADER_GLOBAL_HF

#include "shaderInterop.h"
#include "shaderInterop_renderer.h"

// Bindless resources
ByteAddressBuffer bindless_buffers[] : register(space1);
Texture2D bindless_textures[] : register(space2);

// Immutable samplers
SamplerState samLinearClamp : register(s100);
SamplerState samLinearWrap  : register(s101);
SamplerState samPointClamp  : register(s102);
SamplerState samPointWrap   : register(s103);

inline ShaderSceneCB GetScene()
{
	return g_xFrame.scene;
}

inline CameraCB GetCamera()
{
	return g_xCamera;
}

inline ShaderGeometry LoadGeometry(uint geometryIndex)
{
	return bindless_buffers[GetScene().geometrybuffer].Load<ShaderGeometry>(geometryIndex * sizeof(ShaderGeometry));
}

inline ShaderMaterial LoadMaterial(uint materialIndex)
{
	return bindless_buffers[GetScene().materialbuffer].Load<ShaderMaterial>(materialIndex * sizeof(ShaderMaterial));
}

inline ShaderMeshInstance LoadInstance(uint instanceIndex)
{
	return bindless_buffers[GetScene().instancebuffer].Load<ShaderMeshInstance>(instanceIndex * sizeof(ShaderMeshInstance));

}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 ApplySRGBCurveFast( float3 x )
{
    return x < 0.0031308 ? 12.92 * x : 1.13005 * sqrt(x - 0.00228) - 0.13448 * x + 0.005719;
}

// These functions avoid pow() to efficiently approximate sRGB with an error < 0.4%.
float3 RemoveSRGBCurveFast( float3 x )
{
    return x < 0.04045 ? x / 12.92 : -7.43605 * x - 31.24297 * sqrt(-0.53792 * x + 1.279924) + 35.34864;
}

#define DEGAMMA(x)		(RemoveSRGBCurveFast(x))
#define GAMMA(x)		(ApplySRGBCurveFast(x))

inline float4 unpack_rgba(in uint value)
{
	float4 retVal;
	retVal.x = (float)((value >> 0u) & 0xFF) / 255.0;
	retVal.y = (float)((value >> 8u) & 0xFF) / 255.0;
	retVal.z = (float)((value >> 16u) & 0xFF) / 255.0;
	retVal.w = (float)((value >> 24u) & 0xFF) / 255.0;
	return retVal;
}


#endif