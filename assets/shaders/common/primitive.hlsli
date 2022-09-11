#ifndef PRIMITIVE_HF
#define PRIMITIVE_HF

#include "global.hlsli"

struct PrimitiveID
{
    uint primitiveIndex;
    uint instanceIndex;
    uint subsetIndex;

    // 1 bit valid flag
    // 23 bit meshletIndex
    // 8  bit meshletPrimitiveIndex
    inline uint Pack()
    {
		ShaderMeshInstance inst = LoadInstance(instanceIndex);
		ShaderGeometry geometry = LoadGeometry(inst.geometryOffset + subsetIndex);

        uint meshletIndex = inst.meshletOffset + geometry.meshletOffset + primitiveIndex / MESHLET_TRIANGLE_COUNT;
	    uint meshletPrimitiveIndex = primitiveIndex % MESHLET_TRIANGLE_COUNT;
 		return (1u << 31u) | meshletPrimitiveIndex | meshletIndex;
    }

    // 1 bit valid flag
    // 23 bit meshletIndex
    // 8  bit meshletPrimitiveIndex
    inline void Unpack(uint value)
    {
        value ^= 1u << 31u;

        uint meshletIndex = value & (~0u >> 9u);
		uint meshletPrimitiveIndex = (value >> 23u) & 0xFF;

        ShaderMeshlet meshlet = LoadMeshlet(meshletIndex);
        primitiveIndex = meshlet.primitiveOffset + meshletPrimitiveIndex;
        instanceIndex = meshlet.instanceIndex;

        ShaderMeshInstance inst = LoadInstance(instanceIndex);
        subsetIndex = meshlet.geometryIndex - inst.geometryOffset;
    }
};

#endif