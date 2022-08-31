#include "model.h"
#include "core\profiler\profiler.h"
#include "renderer\renderer.h"
#include "core\resource\resourceManager.h"

namespace VulkanTest
{
	Mesh::Mesh(const GPU::InputLayout& inputLayout_, U8 stride_, const char* name_, const AttributeSemantic* semantics_):
		inputLayout(inputLayout_),
		stride(stride_),
		name(name_)
	{
		if (semantics_)
		{
			for (U32 i = 0; i < inputLayout.attributeCount; i++)
				semantics[i] = semantics_[i];
		}
	}

	bool Mesh::CreateRenderData()
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

		aabb = AABB(_min, _max);

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

		return true;
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

	DEFINE_RESOURCE(Model);

	const U32 Model::FILE_MAGIC = 0x5f4c4d4f;
	const U32 Model::FILE_VERSION = 0x01;

	Model::Model(const Path& path_, ResourceFactory& resFactory_) :
		BinaryResource(path_, resFactory_)
	{
	}

	Model::~Model()
	{
	}

	bool Model::Init(ResourceInitData& initData)
	{
		InputMemoryStream inputMem(initData.customData);
		inputMem.Read<FileHeader>(header);
		if (header.magic != FILE_MAGIC)
		{
			Logger::Warning("Unsupported model file %s", GetPath());
			return false;
		}
		if (header.version != FILE_VERSION)
		{
			Logger::Warning("Unsupported version of model %s", GetPath());
			return false;
		}

		return true;
	}

	bool Model::Load()
	{
		PROFILE_FUNCTION();

		const auto dataChunk = GetChunk(0);
		if (dataChunk == nullptr || !dataChunk->IsLoaded())
			return false;
	
		InputMemoryStream inputMem(dataChunk->Data(), dataChunk->Size());
		if (!ParseMeshes(inputMem))
		{
			Logger::Warning("Invalid model file %s", GetPath());
			return false;
		}

		return true;
	}

	void Model::Unload()
	{
		for (auto& mesh : meshes)
		{
			for (auto& subset : mesh.subsets)
			{
				RemoveDependency(*subset.material);
				subset.material.reset();
			}
		}
		meshes.clear();
	}

	static U8 GetIndexBySemantic(Mesh::AttributeSemantic semantic) 
	{
		switch (semantic)
		{
			case Mesh::AttributeSemantic::POSITION: return 0;
			case Mesh::AttributeSemantic::NORMAL: return 1;
			case Mesh::AttributeSemantic::TEXCOORD0: return 2;
			case Mesh::AttributeSemantic::COLOR0: return 3;
			default: ASSERT(false); return 0;
		}
	}

	static VkFormat formatMap[(U32)Mesh::AttributeType::COUNT][4] = {
		{VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32A32_SFLOAT}, // F32,
		{VK_FORMAT_R32_SINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32A32_SINT}, // I32,
	};
	static VkFormat GetFormat(Mesh::AttributeType type, U8 compCount)
	{
		return formatMap[(U32)type][compCount - 1];
	}

	static int GetTypeSize(Mesh::AttributeType type)
	{
		switch (type) 
		{
			case Mesh::AttributeType::F32: return 4;
			case Mesh::AttributeType::I32: return 4;
			default: ASSERT(false); return 0;
		}
	}

	bool Model::ParseMeshes(InputMemoryStream& mem)
	{
		// Meshes format:
		// -------------------------
		// MeshCount
		// Mesh1
		// Mesh2
		// -- Mesh name
		// -- Matrial path
		// -- AttrCount
		// -- Attr1 (Semantic, Type, Count)
		// -- Attr2 (Semantic, Type, Count)
		// geometry
		// -- Indices
		// -- VertexData
		// ---- Count
		// ---- Vertex
		// ---- Normals
		// ---- tagents
		// ---- Texcoords

		I32 meshCount = 0;
		mem.Read(meshCount);
		if (meshCount <= 0)
			return false;

		// Read meshes
		meshes.reserve(meshCount);
		for (int i = 0; i < meshCount; i++)
		{
			// Read mesh name
			I32 strSize;
			mem.Read(strSize);
			char meshName[MAX_PATH_LENGTH];
			meshName[strSize] = 0;
			mem.Read(meshName, strSize);

			// Read attributes
			U32 attrCount;
			mem.Read(attrCount);

			Mesh::AttributeSemantic semantics[GPU::InputLayout::MAX_ATTRIBUTES];
			for (auto& i : semantics)
				i = Mesh::AttributeSemantic::NONE;

			GPU::InputLayout layout = {};
			U8 offset = 0;
			for (U32 j = 0; j < attrCount; j++)
			{
				Mesh::AttributeType type;
				U8 compCount;
				mem.Read(semantics[j]);
				mem.Read(type);
				mem.Read(compCount);

				const U8 attrIdx = GetIndexBySemantic(semantics[j]);
				const auto format = GetFormat(type, compCount);
				layout.AddAttribute(format, offset);

				offset += GetTypeSize(type) * compCount;
			}

			// Read mesh subsets
			I32 subsetCount = 0;
			mem.Read(subsetCount);

			Array<Mesh::MeshSubset> subsets;
			for (int j = 0; j < subsetCount; j++)
			{
				Mesh::MeshSubset subset = {};

				// Read material path
				I32 matPathSize;
				mem.Read(matPathSize);
				char matPath[MAX_PATH_LENGTH];
				matPath[matPathSize] = 0;
				mem.Read(matPath, matPathSize);

				// Create mesh instance
				ResPtr<Material> material = GetResourceManager().LoadResourcePtr<Material>(Path(matPath));
				AddDependency(*material);
				subset.material = material;

				mem.Read(subset.indexOffset);
				mem.Read(subset.indexCount);

				subsets.push_back(subset);
			}

			meshes.emplace(layout, offset, meshName, semantics);
			meshes.back().subsets = std::move(subsets);
		}

		// Read indices
		for (auto& mesh : meshes)
		{
			I32 indexSize;
			I32 indicesCount;
			mem.Read(indexSize);

			// Index only U32 now
			// if (indexSize != 2 && indexSize != 4)
			if (indexSize != 4)
				return false;
			mem.Read(indicesCount);
			if (indicesCount <= 0)
				return false;

			mesh.indices.resize(indicesCount);
			mem.Read(mesh.indices.data(), sizeof(U32) * indicesCount);
		}

		// Read vertex datas
		for (auto& mesh : meshes)
		{
			U32 vertexCount;
			mem.Read(vertexCount);

			for (U32 i = 0; i < mesh.inputLayout.attributeCount; i++)
			{
				switch (mesh.semantics[i])
				{
				case Mesh::AttributeSemantic::POSITION:
					mesh.vertexPos.resize(vertexCount);
					mem.Read(mesh.vertexPos.data(), sizeof(F32x3) * vertexCount);
					break;
				case Mesh::AttributeSemantic::NORMAL:
					mesh.vertexNor.resize(vertexCount);
					mem.Read(mesh.vertexNor.data(), sizeof(F32x3) * vertexCount);
					break;
				case Mesh::AttributeSemantic::TEXCOORD0:
					mesh.vertexUV.resize(vertexCount);
					mem.Read(mesh.vertexUV.data(), sizeof(F32x2) * vertexCount);
					break;
				default:
					ASSERT(false);
					break;
				}
			}
		}

		// Create render datas
		for (auto& mesh : meshes)
		{
			if (!mesh.CreateRenderData())
				return false;
		}

		return true;
	}
}