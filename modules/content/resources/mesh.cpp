#include "model.h"
#include "core\profiler\profiler.h"
#include "content\resourceManager.h"
#include "core\streaming\streamingHandler.h"

namespace VulkanTest
{
	Mesh::Mesh(const ScriptingObjectParams& params) :
		ScriptingObject(params)
	{
	}

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
		GPU::DeviceVulkan* device = GPU::GPUDevice::Instance;
		GPU::BufferCreateInfo bufferInfo = {};
		bufferInfo.domain = GPU::BufferDomain::Device;
		bufferInfo.usage =
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
			VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;

		// General buffer layout
		//------------------------------------
		// VertexPosNor

		U64 alignment = device->GetMinOffsetAlignment();
		U64 totalSize =
			AlignTo(indices.size() * GetIndexStride(), alignment) +
			AlignTo(vertexPos.size() * sizeof(VertexPosNor), alignment) +
			AlignTo(vertexTangents.size() * sizeof(F32x4), alignment) +
			AlignTo(vertexUV.size() * sizeof(F32x4), alignment);

		OutputMemoryStream output;
		output.Reserve(totalSize);

		// Create index buffer
		auto indexBufferFormat = GetIndexFormat();
		if (indexBufferFormat == GPU::IndexBufferFormat::UINT32)
		{
			ib.offset = output.Size();
			ib.size = indices.size() * sizeof(U32);
			output.Write(indices.data(), ib.size, alignment);
		}
		else
		{
			ib.offset = output.Size();
			ib.size = indices.size() * sizeof(U16);
			U16* indexdata = (U16*)(output.Data() + output.Size());
			output.Skip(AlignTo(ib.size, alignment));
			for (size_t i = 0; i < indices.size(); ++i)
				indexdata[i] = (U16)indices[i];
		}

		F32x3 _min = F32x3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		F32x3 _max = F32x3(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());

		// VertexBuffer position and normal
		vbPosNor.offset = output.Size();
		vbPosNor.size = vertexPos.size() * sizeof(VertexPosNor);
		VertexPosNor* vertices = (VertexPosNor*)(output.Data() + output.Size());
		output.Skip(AlignTo(vbPosNor.size, alignment));

		for (U32 i = 0; i < vertexPos.size(); i++)
		{
			const F32x3& pos = vertexPos[i];
			F32x3 nor = vertexNor.empty() ? F32x3(1.0f) : vertexNor[i];
			nor = StoreF32x3(Vector3Normalize(LoadF32x3(nor)));
			vertices[i].Setup(pos, nor);

			_min = Min(_min, pos);
			_max = Max(_max, pos);
		}

		// VertexBuffer tangent
		if (!vertexTangents.empty())
		{
			vbTan.offset = output.Size();
			vbTan.size = vertexTangents.size() * sizeof(F32x4);
			output.Write(vertexTangents.data(), vbTan.size, alignment);
		}

		// VertexBuffer uvs
		if (!vertexUV.empty())
		{
			vbUVs.offset = output.Size();
			vbUVs.size = vertexUV.size() * sizeof(F32x4);
			F32x4* uvSets = (F32x4*)(output.Data() + output.Size());
			output.Skip(AlignTo(vbUVs.size, alignment));

			for (int i = 0; i < vertexUV.size(); i++)
				uvSets[i] = F32x4(vertexUV[i].x, vertexUV[i].y, 0.0f, 0.0f);
		}

		bufferInfo.size = output.Size();
		auto buffer = device->CreateBuffer(bufferInfo, output.Data());
		if (!buffer)
			return false;
		device->SetName(*buffer, "MeshBuffer");
		generalBuffer = buffer;

		// Create bindless descriptor
		vbPosNor.srv = device->CreateBindlessStroageBuffer(*buffer, vbPosNor.offset, vbPosNor.size);
		vbUVs.srv = vbUVs.size > 0 ? device->CreateBindlessStroageBuffer(*buffer, vbUVs.offset, vbUVs.size) : GPU::BindlessDescriptorPtr();
		vbTan.srv = vbTan.size > 0 ? device->CreateBindlessStroageBuffer(*buffer, vbTan.offset, vbTan.size) : GPU::BindlessDescriptorPtr();

		GPU::BufferViewCreateInfo viewInfo = {};
		viewInfo.buffer = buffer.get();
		viewInfo.offset = ib.offset;
		viewInfo.range = ib.size;
		viewInfo.format = GetIndexFormat() == GPU::IndexBufferFormat::UINT32 ? VK_FORMAT_R32_UINT : VK_FORMAT_R16_UINT;
		ibView = device->CreateBufferView(viewInfo);
		ib.srv = device->CreateBindlessUniformTexelBuffer(*buffer, *ibView);

		return true;
	}

	void Mesh::Unload()
	{
		generalBuffer.reset();
		vbPosNor.Reset();
		vbUVs.Reset();
		vbTan.Reset();
		ib.Reset();
		ibView.reset();
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