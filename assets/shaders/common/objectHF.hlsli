#ifndef SHADER_OBJECT_HF
#define SHADER_OBJECT_HF

#include "global.hlsli"
#include "surface.hlsli"
#include "lighting.hlsli"

////////////////////////////////////////////////////////////////////////////////////
// Use these to define the expected layout for the shader:
// #define OBJECTSHADER_LAYOUT to set layout
#define OBJECTSHADER_LAYOUT_COMMON	1	//	- layout for common passes
#define OBJECTSHADER_LAYOUT_PREPASS	2	//	- layout for prepass

#if OBJECTSHADER_LAYOUT == OBJECTSHADER_LAYOUT_PREPASS
#define OBJECTSHADER_USE_INSTANCEINDEX
#endif

#if OBJECTSHADER_LAYOUT == OBJECTSHADER_LAYOUT_COMMON
#define OBJECTSHADER_USE_COLOR
#define OBJECTSHADER_USE_UVSETS
#define OBJECTSHADER_USE_NORMAL
#define OBJECTSHADER_USE_TANGENT
#define OBJECTSHADER_USE_POSITION3D
#define OBJECTSHADER_USE_INSTANCEINDEX
#endif
////////////////////////////////////////////////////////////////////////////////////

PUSHCONSTANT(push, ObjectPushConstants)

inline ShaderGeometry GetMesh()
{
	return LoadGeometry(push.geometryIndex);
}

inline ShaderMaterial GetMaterial()
{
    return LoadMaterial(push.materialIndex);
}

// Bindless textures
#define texture_basecolormap bindless_textures[GetMaterial().texture_basecolormap_index]
#define texture_normalmap bindless_textures[GetMaterial().texture_normalmap_index]

// VertexShaderInput
struct VertexInput
{
	uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;

	float4 GetPosition()
	{
		return float4(bindless_buffers[GetMesh().vbPosNor].Load<float3>(vertexID * sizeof(uint4)), 1);
	}

    float3 GetNormal()
    {
        const uint normalUint = bindless_buffers[GetMesh().vbPosNor].Load<uint4>(vertexID * sizeof(uint4)).w;
		float3 normal;
		normal.x = (float)((normalUint >> 0u) & 0xFF) / 255.0 * 2 - 1;
		normal.y = (float)((normalUint >> 8u) & 0xFF) / 255.0 * 2 - 1;
		normal.z = (float)((normalUint >> 16u) & 0xFF) / 255.0 * 2 - 1;
		return normal;
    }

    float4 GetTangent()
    {
		[branch]
		if (GetMesh().vbTan < 0)
			return 0;

        return bindless_buffers[GetMesh().vbTan].Load<float4>(vertexID * sizeof(float4));
    }

    float2 GetUVSets()
    {   
        [branch]
		if (GetMesh().vbUVs < 0)
			return 0;
        
        return bindless_buffers[GetMesh().vbUVs].Load<float4>(vertexID * sizeof(float4)).xy;
    }

    ShaderMeshInstancePointer GetInstancePointer()
	{
		if (push.instance >= 0)
			return bindless_buffers[push.instance].Load<ShaderMeshInstancePointer>(push.instanceOffset + instanceID * sizeof(ShaderMeshInstancePointer));

		ShaderMeshInstancePointer pointer;
		pointer.init();
		return pointer;
	}

    ShaderMeshInstance GetInstance()
    {
        if (push.instance >= 0)
            return LoadInstance(GetInstancePointer().instanceIndex);

        ShaderMeshInstance inst;
        inst.init();
        return inst;
    }
};

// VertexSurface (In VertexShader)
struct VertexSurface
{
    float4 position;
    float2 uv;
    float3 normal;
    float4 tangent;
	float4 color;

    inline void Create(in VertexInput input)
    {
        position = input.GetPosition();
        color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        uv = input.GetUVSets();
        
        float3x3 transforInvTranspose = (float3x3)input.GetInstance().transformInvTranspose.GetMatrix();
        normal = normalize(mul(transforInvTranspose, input.GetNormal()));

        tangent = input.GetTangent();   
        tangent.xyz = normalize(mul(transforInvTranspose, tangent.xyz));
    
        position = mul(input.GetInstance().transform.GetMatrix(), position);
    }
};

struct PixelInput
{
    float4 pos : SV_POSITION;

#ifdef OBJECTSHADER_USE_UVSETS
	float4 uvsets : UVSETS;
#endif

#ifdef OBJECTSHADER_USE_COLOR
	float4 color : COLOR;
#endif

#ifdef OBJECTSHADER_USE_POSITION3D
	float3 pos3D : WORLDPOSITION;
#endif

#ifdef OBJECTSHADER_USE_NORMAL
	float3 nor : NORMAL;
#endif

#ifdef OBJECTSHADER_USE_TANGENT
    float4 tan : TANGENT;
#endif

#ifdef OBJECTSHADER_USE_INSTANCEINDEX
	uint instanceIndex : INSTANCEINDEX;
#endif
};




#endif