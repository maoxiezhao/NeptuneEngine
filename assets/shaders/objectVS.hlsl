#include "common/global.hlsli"

PUSHCONSTANT(push, ObjectPushConstants)

inline ShaderGeometry GetMesh()
{
	return LoadGeometry(push.geometryIndex);
}

struct VertexInput
{
	uint vertexID : SV_VertexID;

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
};

struct PixelInput
{
    float4 pos : SV_POSITION;
};

PixelInput main(VertexInput input)
{
    PixelInput Out;
    Out.pos = input.GetPosition();
    Out.pos = mul(GetCamera().viewProjection, Out.pos);
    return Out;
}