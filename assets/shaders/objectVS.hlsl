#include "common/global.hlsli"

PUSHCONSTANT(push, ObjectPushConstants)

inline ShaderGeometry GetMesh()
{
	return LoadGeometry(push.geometryIndex);
}

struct VertexInput
{
	uint vertexID : SV_VertexID;
    uint instanceID : SV_InstanceID;

	float4 GetPosition()
	{
		return float4(bindless_buffers[GetMesh().vbPos].Load<float3>(vertexID * sizeof(float3)), 1);
	}

    float3 GetNormal()
    {
        return bindless_buffers[GetMesh().vbNor].Load<float3>(vertexID * sizeof(float3));
    }

    float2 GetUVSets()
    {   
        [branch]
		if (GetMesh().vbUVs < 0)
			return 0;
        
        return bindless_buffers[GetMesh().vbUVs].Load<float2>(vertexID * sizeof(float2));
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

struct VertexSurface
{
    float4 position;
    float2 uv;
    float3 normal;

    inline void Create(in VertexInput input)
    {
        position = input.GetPosition();
        uv = input.GetUVSets();
        normal = input.GetNormal();

        position = mul(input.GetInstance().transform.GetMatrix(), position);
    }
};

struct PixelInput
{
    float4 pos : SV_POSITION;
};

PixelInput main(VertexInput input)
{
    VertexSurface surface;
    surface.Create(input);

    PixelInput Out;
    Out.pos = surface.position;
    Out.pos = mul(GetCamera().viewProjection, Out.pos);
    return Out;
}