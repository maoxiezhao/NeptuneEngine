#ifndef SHADER_INTEROP_FONT
#define SHADER_INTEROP_FONT

#include "shaderInterop.h"

#define CBSLOT_FONT 0

struct FontCB
{
	int bufferIndex;
	uint bufferOffset;
	int textureIndex;
	uint color;
	float4x4 transform;
};
CONSTANTBUFFER(font, FontCB, CBSLOT_FONT);

#endif