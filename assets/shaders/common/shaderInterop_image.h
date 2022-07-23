#ifndef SHADER_INTEROP_IMAGE
#define SHADER_INTEROP_IMAGE

#include "shaderInterop.h"

#define CBSLOT_IMAGE 0

static const uint IMAGE_FLAG_FULLSCREEN = 1 << 0;

struct ImageCB
{
	float4 corners0;
	float4 corners1;
	float4 corners2;
	float4 corners3;

	uint  flags;
	float padding0;
	float padding1;
	float padding2;
};
CONSTANTBUFFER(image, ImageCB, CBSLOT_IMAGE);

#endif