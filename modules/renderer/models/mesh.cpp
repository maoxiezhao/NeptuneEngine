#include "model.h"
#include "core\profiler\profiler.h"
#include "core\resource\resourceManager.h"
#include "core\streaming\streamingHandler.h"
#include "renderer\renderer.h"

namespace VulkanTest
{
	void Mesh::Init(const char* name_, Model* model_, I32 lodIndex_, I32 index_, const AABB& aabb_)
	{
		name = name_;
		model = model_;
		lodIndex = lodIndex_;
		index = index_;
		aabb = aabb_;
	}

	bool Mesh::Load()
	{
		GPU::DeviceVulkan* device = Renderer::GetDevice();
		GPU::BufferCreateInfo bufferInfo = {};
		bufferInfo.domain = GPU::BufferDomain::Device;
		bufferInfo.usage =
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

		U64 alignment = device->GetMinOffsetAlignment();
		U64 totalSize =
			AlignTo(indices.size() * sizeof(U32), alignment) +
			AlignTo(vertexPos.size() * sizeof(F32x3), alignment) +
			AlignTo(vertexNor.size() * sizeof(F32x3), alignment) +
			AlignTo(vertexUV.size() * sizeof(F32x2), alignment);

		OutputMemoryStream output;
		output.Reserve(totalSize);

		// Create index buffer
		ib.offset = output.Size();
		ib.size = indices.size() * sizeof(U32);
		output.Write(indices.data(), ib.size, alignment);

		F32x3 _min = F32x3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		F32x3 _max = F32x3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

		// VertexBuffer position
		vbPos.offset = output.Size();
		vbPos.size = vertexPos.size() * sizeof(F32x3);
		output.Write(vertexPos.data(), vbPos.size, alignment);
		for (U32 i = 0; i < vertexPos.size(); i++)
		{
			const F32x3& pos = vertexPos[i];
			_min = Min(_min, pos);
			_max = Max(_max, pos);
		}

		// VertexBuffer normal
		if (!vertexNor.empty())
		{
			vbNor.offset = output.Size();
			vbNor.size = vertexNor.size() * sizeof(F32x3);
			output.Write(vertexNor.data(), vbNor.size, alignment);
		}

		// VertexBuffer tangent
		if (!vertexTangents.empty())
		{
			vbTan.offset = output.Size();
			vbTan.size = vertexTangents.size() * sizeof(F32x3);
			output.Write(vertexTangents.data(), vbTan.size, alignment);
		}

		// VertexBuffer uvs
		if (!vertexUV.empty())
		{
			vbUVs.offset = output.Size();
			vbUVs.size = vertexUV.size() * sizeof(F32x2);
			output.Write(vertexUV.data(), vbUVs.size, alignment);
		}

		bufferInfo.size = output.Size();
		auto buffer = device->CreateBuffer(bufferInfo, output.Data());
		if (!buffer)
			return false;
		device->SetName(*buffer, "MeshBuffer");
		generalBuffer = buffer;

		// Create bindless descriptor
		vbPos.srv = device->CreateBindlessStroageBuffer(*buffer, vbPos.offset, vbPos.size);
		vbNor.srv = device->CreateBindlessStroageBuffer(*buffer, vbNor.offset, vbNor.size);
		vbUVs.srv = device->CreateBindlessStroageBuffer(*buffer, vbUVs.offset, vbUVs.size);
		vbTan.srv = device->CreateBindlessStroageBuffer(*buffer, vbTan.offset, vbTan.size);

		return true;
	}

	void Mesh::Unload()
	{
		generalBuffer.reset();
		vbPos.Reset();
		vbNor.Reset();
		vbUVs.Reset();
		vbTan.Reset();
	}

	bool Mesh::IsReady()const
	{
		return (bool)generalBuffer;
	}

	PickResult Mesh::CastRayPick(const VECTOR& rayOrigin, const VECTOR& rayDirection, F32 tmin, F32 tmax)
	{
		PickResult ret = {};
		LODMeshIndices lodMeshIndex = GetLODMeshIndex(0);
		for (int index = lodMeshIndex.from; index < lodMeshIndex.to; index++)
		{
			const auto& subset = subsets[index];
			for (U32 i = 0; i < subset.indexCount; i += 3)
			{
				const U32 i0 = indices[subset.indexOffset + i + 0];
				const U32 i1 = indices[subset.indexOffset + i + 1];
				const U32 i2 = indices[subset.indexOffset + i + 2];

				VECTOR p0 = LoadF32x3(vertexPos[i0]);
				VECTOR p1 = LoadF32x3(vertexPos[i1]);
				VECTOR p2 = LoadF32x3(vertexPos[i2]);

				F32 dist;
				if (RayTriangleIntersects(rayOrigin, rayDirection, p0, p1, p2, dist, tmin, tmax))
				{
					if (dist < ret.distance)
					{
						ret.isHit = true;
						ret.distance = dist;
						ret.normal = StoreF32x3(Vector3Cross(VectorSubtract(p2, p1), VectorSubtract(p1, p0)));
					}
				}
			}
		}
		return ret;
	}
}