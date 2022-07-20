#include "common/global.hlsli"
#include "common/shaderInterop_postprocess.h"

Texture2D<float4> input : register(t0);
RWTexture2D<float4> output : register(u0);

PUSHCONSTANT(push, PostprocessPushConstants)

static const int GAUSS_KERNEL = 9;
static const float gaussianWeightsNormalized[GAUSS_KERNEL] = {
	0.004112,
	0.026563,
	0.100519,
	0.223215,
	0.29118,
	0.223215,
	0.100519,
	0.026563,
	0.004112
};
static const int gaussianOffsets[GAUSS_KERNEL] = {
	-4,
	-3,
	-2,
	-1,
	0,
	1,
	2,
	3,
	4
};

static const int TILE_BORDER = GAUSS_KERNEL / 2;
static const int CACHE_SIZE = TILE_BORDER + POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT + TILE_BORDER;
groupshared float4 color_cache[CACHE_SIZE];

[numthreads(POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT, 1, 1)]
void main(uint3 Gid : SV_GroupID, uint groupIndex : SV_GroupIndex)
{	
    float2 direction = push.params0.xy;
	const bool horizontal = direction.y == 0;
    uint2 startPos = Gid.xy;
	[flatten]
	if (horizontal)
		startPos.x *= POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT;
	else
		startPos.y *= POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT;

    int i;
    for (i = groupIndex; i < CACHE_SIZE; i += POSTPROCESS_BLUR_GAUSSIAN_THREADCOUNT)
    {
        const float2 uv = (startPos + 0.5f + direction * (i - TILE_BORDER)) * push.resolution_rcp;
        color_cache[i] = input.SampleLevel(samLinearClamp, uv, 0);
    }
    GroupMemoryBarrierWithGroupSync();

    const uint2 pixel = startPos + groupIndex * direction;
	if (pixel.x >= push.resolution.x || pixel.y >= push.resolution.y)
		return;

    const uint center = TILE_BORDER + groupIndex;
    float4 finalColor = 0;
    for (i = 0; i < GAUSS_KERNEL; i++)
    {
        const uint pos = center + gaussianOffsets[i];
        const float4 color = color_cache[pos];
        finalColor += color * gaussianWeightsNormalized[i];
    }
    output[pixel] = finalColor;
}