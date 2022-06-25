#include "model.h"
#include "core\utils\profiler.h"
#include "renderer\renderer.h"
#include "core\resource\resourceManager.h"

namespace VulkanTest
{
	Mesh::Mesh(ResPtr<Material>&& mat_, const GPU::InputLayout& inputLayout_, U8 stride_, const char* name_, const AttributeSemantic* semantics_):
		material(std::move(mat_)),
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

	void Mesh::CreateRenderData()
	{
		GPU::DeviceVulkan* device = Renderer::GetDevice();
		GPU::BufferCreateInfo bufferInfo = {};
	
		
	}

	DEFINE_RESOURCE(Model);

	const U32 Model::FILE_MAGIC = 0x5f4c4d4f;
	const U32 Model::FILE_VERSION = 0x01;

	Model::Model(const Path& path_, ResourceFactory& resFactory_) :
		Resource(path_, resFactory_)
	{
	}

	Model::~Model()
	{
	}

	bool Model::OnLoaded(U64 size, const U8* mem)
	{
		PROFILE_FUNCTION();
		FileHeader header;

		InputMemoryStream inputMem(mem, size);
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

		if (!ParseMeshes(inputMem))
		{
			Logger::Warning("Invalid model file %s", GetPath());
			return false;
		}

		return true;
	}

	void Model::OnUnLoaded()
	{
		for (auto& mesh : meshes)
		{
			RemoveDependency(*mesh.material);
			mesh.material.reset();
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

			// Read material path
			I32 matPathSize;
			mem.Read(matPathSize);
			char matPath[MAX_PATH_LENGTH];
			matPath[matPathSize] = 0;
			mem.Read(matPath, matPathSize);

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

			U32 stride = offset;

			// Create mesh instance
			ResPtr<Material> material = GetResourceManager().LoadResourcePtr<Material>(Path(matPath));
			AddDependency(*material);

			meshes.emplace(std::move(material), layout, stride, meshName, semantics);
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

			mesh.indices.resize(sizeof(U32) * indicesCount);
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
				case Mesh::AttributeSemantic::COLOR0:
					mesh.vertexColor.resize(vertexCount);
					mem.Read(mesh.vertexColor.data(), sizeof(U32) * vertexCount);
					break;
				default:
					break;
				}
			}
		}

		// Create render datas
		for (auto& mesh : meshes)
			mesh.CreateRenderData();

		return true;
	}
}