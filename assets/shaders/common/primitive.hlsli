#ifndef PRIMITIVE_HF
#define PRIMITIVE_HF

#include "global.hlsli"

struct PrimitiveID
{
    uint primitiveIndex;
    uint instanceIndex;
    uint subsetIndex;

    // |---------------------- 32 bits-----------------------|
    // | 1 valid | 8 meshletPrimitiveIndex | 23 meshletIndex |
    inline uint Pack()
    {
        ShaderMeshInstance inst = LoadInstance(instanceIndex);
        ShaderGeometry geometry = LoadGeometry(inst.geometryOffset + subsetIndex);

        // MeshletIndex
		uint meshletIndex = inst.meshletOffset + geometry.meshletOffset + primitiveIndex / MESHLET_TRIANGLE_COUNT;
		meshletIndex &= ~0u >> 9u;

        // Meshlet primitive index
		uint meshletPrimitiveIndex = primitiveIndex % MESHLET_TRIANGLE_COUNT;
		meshletPrimitiveIndex &= 0xFF;
		meshletPrimitiveIndex <<= 23u;

        return (1u << 31u) | meshletPrimitiveIndex | meshletIndex;
    }

    // |---------------------- 32 bits-----------------------|
    // | 1 valid | 8 meshletPrimitiveIndex | 23 meshletIndex |
    inline void Unpack(uint value)
    {
		value ^= 1u << 31u; // remove valid flag
		uint meshletIndex = value & (~0u >> 9u);
		uint meshletPrimitiveIndex = (value >> 23u) & 0xFF;
		ShaderMeshlet meshlet = LoadMeshlet(meshletIndex);
		ShaderMeshInstance inst = LoadInstance(meshlet.instanceIndex);
		primitiveIndex = meshlet.primitiveOffset + meshletPrimitiveIndex;
		instanceIndex = meshlet.instanceIndex;
		subsetIndex = meshlet.geometryIndex - inst.geometryOffset;
    }
};

#endif