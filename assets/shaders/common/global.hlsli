#ifndef SHADER_GLOBAL
#define SHADER_GLOBAL

#include "shaderInterop.h"

ByteAddressBuffer bindless_buffers[] : register(space1);

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



#endif