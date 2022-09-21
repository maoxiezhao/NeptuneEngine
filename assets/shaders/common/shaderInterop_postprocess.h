#ifndef SHADER_INTEROP_POSTPROCESS
#define SHADER_INTEROP_POSTPROCESS

#include "shaderInterop.h"

static const uint POSTPROCESS_BLOCKSIZE = 8;
static const uint POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT = 256;

struct PostprocessPushConstants
{
	uint2 resolution;
	float2 resolution_rcp;
	float4 params0;
	float4 params1;
};

struct TonemapPushConstants
{
	float2 resolution_rcp;
	float exposure;
	float dither;
};

#endif