#ifndef SHADER_INTEROP_RENDERER
#define SHADER_INTEROP_RENDERER

#include "shaderInterop.h"

// Shader light type
enum SHADER_LIGHT_TYPE
{
	SHADER_LIGHT_TYPE_DIRECTIONALLIGHT,
	SHADER_LIGHT_TYPE_POINTLIGHT,
	SHADER_LIGHT_TYPE_SPOTLIGHT,
	SHADER_LIGHT_TYPE_COUNT
};

// 256 triangle batch of a ShaderGeometry
static const uint MESHLET_TRIANGLE_COUNT = 256u;
inline uint TriangleCountToMeshletCount(uint triangleCount)
{
	return (triangleCount + MESHLET_TRIANGLE_COUNT - 1u) / MESHLET_TRIANGLE_COUNT;
}
struct ShaderMeshlet
{
	uint instanceIndex;
	uint geometryIndex;
	uint primitiveOffset;
	uint padding;
};

// Visibility
static const uint VISIBILITY_BLOCKSIZE = 8;

// Max light count in per frame
static const uint SHADER_ENTITY_COUNT = 256;
static const uint SHADER_ENTITY_TILE_BUCKET_COUNT = SHADER_ENTITY_COUNT / 32;

// Light culling
static const uint TILED_CULLING_BLOCK_SIZE = 16;
static const uint TILED_CULLING_THREADSIZE = 8;

struct ShaderSceneCB
{
	int geometrybuffer;
	int materialbuffer;
	int instancebuffer;
	int meshletBuffer;
};

struct FrameCB
{
	ShaderSceneCB scene;

	uint lightOffset;
	uint lightCount;

	int bufferShaderLightsIndex;
};
CONSTANTBUFFER(g_xFrame, FrameCB, CBSLOT_RENDERER_FRAME);

struct CameraCB
{
	float4x4 view;
	float4x4 viewProjection;
	float4x4 invProjection;
	float4x4 invViewProjection;

	float3 position;
	float zNear;
	
	float3 forward;
	float zFar;

	uint2 resolution;
	float2 resolutionRcp;

	uint3 cullingTileCount;		// Entity culling tile
	uint padding;

	int textureDepthIndex;	//  Depth texture bindless index
	int cullingTileBufferIndex; // Entity culling tile buffer bindless index
};

CONSTANTBUFFER(g_xCamera, CameraCB, CBSLOT_RENDERER_CAMERA);

struct ShaderGeometry
{
	int vbPos;
	int vbNor;
	int vbTan;
	int vbUVs;
	int ib;
	uint indexOffset;

	uint meshletOffset;
	uint meshletCount;
};

struct ShaderMaterial
{
	float4 baseColor;
	float  roughness;
	float  metalness;
	float  reflectance;

	int	   texture_basecolormap_index;
	int    texture_normalmap_index;
};

struct ObjectPushConstants
{
	uint geometryIndex;
	uint materialIndex;
	int  instance;
	uint instanceOffset;
};

struct ShaderTransform
{
	float4 mat0;
	float4 mat1;
	float4 mat2;

	void init()
	{
		mat0 = float4(1, 0, 0, 0);
		mat1 = float4(0, 1, 0, 0);
		mat2 = float4(0, 0, 1, 0);
	}
	void Create(float4x4 mat)
	{
		mat0 = float4(mat._11, mat._21, mat._31, mat._41);
		mat1 = float4(mat._12, mat._22, mat._32, mat._42);
		mat2 = float4(mat._13, mat._23, mat._33, mat._43);
	}
	float4x4 GetMatrix()
#ifdef __cplusplus
		const
#endif // __cplusplus
	{
		return float4x4(
			mat0.x, mat0.y, mat0.z, mat0.w,
			mat1.x, mat1.y, mat1.z, mat1.w,
			mat2.x, mat2.y, mat2.z, mat2.w,
			0, 0, 0, 1
		);
	}
};

struct ShaderMeshInstance
{
	uint uid;
	uint geometryOffset;
	uint geometryCount;
	uint meshletOffset;

	ShaderTransform transform;
	ShaderTransform transformInvTranspose; // Transform normal

	void init()
	{
		
		meshletOffset = 0;

		transform.init();
		transformInvTranspose.init();
	}
};

struct ShaderMeshInstancePointer
{
	uint instanceIndex;
	void init()
	{
		instanceIndex = 0xFFFFFF;
	}
};

struct ShaderLight
{
	float3 position;
	uint type16_range16;
	uint2 direction;
	uint2 color;

#ifndef __cplusplus
	inline uint GetType() {
		return type16_range16 & 0xffff;
	}

	inline float GetRange() {
		return f16tof32(type16_range16 >> 16u);
	}

	inline float3 GetDirection()
	{
		return normalize(float3(
			f16tof32(direction.x),
			f16tof32(direction.x >> 16u),
			f16tof32(direction.y)
		));
	}

	inline float4 GetColor()
	{
		float4 ret;
		ret.x = f16tof32(color.x);
		ret.y = f16tof32(color.x >> 16u);
		ret.z = f16tof32(color.y);
		ret.w = f16tof32(color.y >> 16u);
		return ret;
	}
#else
	inline void SetType(uint type) {
		type16_range16 |= type & 0xffff;
	}

	inline void SetRange(float range) {
		type16_range16 |= VulkanTest::ConvertFloatToHalf(range) << 16u;
	}

	inline void SetDirection(float3 value)
	{
		direction.x |= VulkanTest::ConvertFloatToHalf(value.x);
		direction.x |= VulkanTest::ConvertFloatToHalf(value.y) << 16u;
		direction.y |= VulkanTest::ConvertFloatToHalf(value.z);
	}

	inline void SetColor(float4 value)
	{
		color.x |= VulkanTest::ConvertFloatToHalf(value.x);
		color.x |= VulkanTest::ConvertFloatToHalf(value.y) << 16u;
		color.y |= VulkanTest::ConvertFloatToHalf(value.z);
		color.y |= VulkanTest::ConvertFloatToHalf(value.w) << 16u;
	}

#endif
};

#endif