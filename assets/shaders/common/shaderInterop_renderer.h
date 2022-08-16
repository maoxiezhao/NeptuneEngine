#ifndef SHADER_INTEROP_RENDERER
#define SHADER_INTEROP_RENDERER

#include "shaderInterop.h"

struct ShaderSceneCB
{
	int geometrybuffer;
	int materialbuffer;
	int instancebuffer;
	int padding;
};

struct FrameCB
{
	ShaderSceneCB scene;
};
CONSTANTBUFFER(g_xFrame, FrameCB, CBSLOT_RENDERER_FRAME);

struct CameraCB
{
	float4x4 viewProjection;
};

CONSTANTBUFFER(g_xCamera, CameraCB, CBSLOT_RENDERER_CAMERA);

struct ShaderGeometry
{
	int vbPos;
	int vbNor;
	int vbUVs;
	int ib;
};

struct ShaderMaterial
{
	float4 baseColor;
	int	   texture_basecolormap_index;
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
	ShaderTransform transform;

	void init()
	{
		transform.init();
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

#endif