#include "modelImporter.h"
#include "modelTool.h"
#include "importModel.h"
#include "editor\editor.h"
#include "editor\importers\texture\textureImporter.h"
#include "editor\importers\material\createMaterial.h"
#include "editor\importers\resourceImportingManager.h"

namespace VulkanTest
{
namespace Editor
{
	void PostprocessModelData(ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg)
	{
		// Calculate vertex tangents
		for (auto& importMesh : modelData.meshes)
		{
			if (!importMesh.vertexPositions.empty() && importMesh.hasUV)
			{
				I32 vertexCount = importMesh.vertexPositions.size();
				importMesh.vertexTangents.resize(vertexCount);
				ModelTool::ComputeVertexTangents(
					importMesh.vertexTangents,
					importMesh.indices.size(), importMesh.indices.data(),
					importMesh.vertexPositions.data(),
					importMesh.vertexNormals.data(),
					importMesh.vertexUvset_0.data());
			}
		}

		// TODO
		// Generate lod data
	}


	bool ImportModelData(const String& path, ModelImporter::ImportModel& modelData, ModelImporter::ImportConfig& cfg)
	{
		Logger::Info("Importing model from %s", path.c_str());
		const auto startTime = Timer::GetTimeSeconds();

		// Import model data
		char ext[5] = {};
		CopyString(Span(ext), Path::GetExtension(path.toSpan()));
		MakeLowercase(Span(ext), ext);
		if (EqualString(ext, "obj"))
		{
			if (!ImportModelDataObj(path.c_str(), modelData, cfg))
				return false;
		}
		else
		{
			Logger::Error("Unsupport model format %s", path.c_str());
			return false;
		}

		// Validate
		if (modelData.lods.empty() || modelData.lods[0].meshes.empty())
		{
			Logger::Error("No invalid geometry");
			return false;
		}

		// Postprocess model data
		PostprocessModelData(modelData, cfg);

		// Perpare textures
		HashMap<Path, bool> importedFiles;
		for (auto& tex : modelData.textures)
		{
			if (!tex.isValid)
				continue;

			auto texPath = Path(tex.path);
			if (!importedFiles.contains(texPath))
			{
				importedFiles.insert(Path(tex.path), true);

				TextureImporter::ImportConfig config = {};
				ResourceImportingManager::Import(texPath, tex.guid, &config);
			}
		}

		// Perpare materials
		const PathInfo pathInfo(path);
		for (auto& material : modelData.materials)
		{
			if (!material.import)
				continue;

			CreateMaterial::Options options = {};
			memset(&options.info, 0, sizeof(MaterialInfo));
			options.info.domain = MaterialDomain::Object;
			options.info.type = MaterialType::Standard;

			// BaseColor
			options.baseColor = Color4(material.color);
			options.metalness = material.metallic;
			options.roughness = material.roughness;

			// Write textures
			auto WriteTexture = [&material, &options](Texture::TextureType type)
			{
				auto& texture = material.textures[(U32)type];
				if (!texture.isValid || texture.path.empty())
					return;

				options.textures[(U32)type] = texture.path.c_str();
			};
			WriteTexture(Texture::TextureType::DIFFUSE);
			WriteTexture(Texture::TextureType::NORMAL);
			WriteTexture(Texture::TextureType::SPECULAR);

			const auto& matName = material.name;
			StaticString<MAX_PATH_LENGTH> matPath(pathInfo.dir, matName.c_str(), ".mat");
			if (!ResourceImportingManager::Create(
				ResourceImportingManager::CreateMaterialTag,
				material.guid,
				Path(matPath),
				&options,
				false))
			{
				Logger::Warning("Faield to create a material %s from model %s", matPath.c_str(), path);
				return false;
			}
		}

		Logger::Info("Model resource imported %s in %.3f s", path.c_str(), Timer::GetTimeSeconds() - startTime);
		return true;
	}

	CreateResult ModelImporter::Import(CreateResourceContext& ctx)
	{
		ImportConfig cfg;
		if (ctx.customArg != nullptr)
			cfg = *static_cast<ImportConfig*>(ctx.customArg);

		// Import model data
		ImportModel importModelData;
		if (!ImportModelData(ctx.input, importModelData, cfg))
		{
			Logger::Error("Failed to import model file %s", ctx.input.c_str());
			return CreateResult::Error;
		}

		// Writer output model
		CreateResult ret = CreateResult::Error;
		switch (cfg.type)
		{
		case ModelType::Model:
			ret = WriteModel(ctx, importModelData);
			break;
		default:
			break;
		}
		return ret;
	}

	void GetImportMeshName(const ModelImporter::ImportMesh& mesh, char(&out)[256])
	{
		CopyString(out, mesh.name.c_str());
	}

	CreateResult ModelImporter::WriteModel(CreateResourceContext& ctx, ImportModel& modelData)
	{
		IMPORT_SETUP(Model);

		// Model header
		Model::FileHeader header;
		header.magic = Model::FILE_MAGIC;
		header.version = Model::FILE_VERSION;
		ctx.WriteCustomData(header);

		// Write base model data in chunk 0
		auto dataChunk = ctx.AllocateChunk(0);
		auto outMem = &dataChunk->mem;

		// Write mateirals
		outMem->Write((I32)modelData.materials.size());
		for (const auto& material : modelData.materials)
		{
			// Mateiral guid
			outMem->Write(material.guid);

			// Mateiral name
			I32 length = material.name.length();
			outMem->Write(length);
			outMem->Write(material.name.c_str(), length);
		}

		// Write lods
		I32 lodsCount = modelData.lods.size();
		outMem->Write((U8)lodsCount);
		for (int i = 0; i < lodsCount; i++)
		{
			auto& lod = modelData.lods[i];
			I32 meshCount = lod.meshes.size();
			outMem->Write((U16)meshCount);

			for (I32 meshIndex = 0; meshIndex < meshCount; meshIndex++)
			{
				const auto& mesh = *lod.meshes[meshIndex];

				// Mesh name
				char name[256];
				GetImportMeshName(mesh, name);
				I32 nameLen = (I32)StringLength(name);
				outMem->Write(nameLen);
				outMem->Write(name, nameLen);

				// AABB
				outMem->WriteAABB(mesh.aabb);

				// Subsets
				outMem->Write(mesh.subsets.size());
				for (const auto& subset : mesh.subsets)
				{
					outMem->Write(subset.materialIndex);
					outMem->Write(subset.uniqueIndexOffset);
					outMem->Write(subset.uniqueIndexCount);
				}
			}
		}

		// Write lod chunk datas
		for (int i = 0; i < modelData.lods.size(); i++)
		{
			auto lodDataChunk = ctx.AllocateChunk(MODEL_LOD_TO_CHUNK_INDEX(i));
			auto lodMem = &lodDataChunk->mem;
			for (auto& mesh : modelData.lods[i].meshes)
				WriteMesh(*lodMem, *mesh);
		}

		return CreateResult::Ok;
	}

	I32 GetAttributeCount(const ModelImporter::ImportMesh& mesh)
	{
		I32 attributeCount = 0;
		if (!mesh.vertexPositions.empty())
			attributeCount++;
		if (!mesh.vertexNormals.empty())
			attributeCount++;
		if (!mesh.vertexUvset_0.empty())
			attributeCount++;
		if (!mesh.vertexTangents.empty())
			attributeCount++;
		return attributeCount; // Pos & Normals & Tagents & UV
	}

	bool AreIndices16Bit(const ModelImporter::ImportMesh& mesh)
	{
		// TODO: mesh only support U32 now..
		// return mesh.indices.size() < (1 << 16);
		return false;
	}

	bool ModelImporter::WriteMesh(OutputMemoryStream& outMem, const ImportMesh& mesh)
	{
		// Mesh format:
		// ----------------------------------
		// AttrCount
		// Attr1 (Semantic, Type, Count)
		// Attr2 (Semantic, Type, Count)
		// Indiecs
		// VertexDatas

		// Attributes (pos, normal, texcoord)
		U32 attrCount = GetAttributeCount(mesh);
		outMem.Write(attrCount);

		if (!mesh.vertexPositions.empty())
		{
			outMem.Write(Mesh::AttributeSemantic::POSITION);
			outMem.Write(Mesh::AttributeType::F32);
			outMem.Write((U8)3);
		}
		if (!mesh.vertexNormals.empty())
		{
			outMem.Write(Mesh::AttributeSemantic::NORMAL);
			outMem.Write(Mesh::AttributeType::F32);
			outMem.Write((U8)3);
		}
		if (!mesh.vertexTangents.empty())
		{
			outMem.Write(Mesh::AttributeSemantic::TANGENT);
			outMem.Write(Mesh::AttributeType::F32);
			outMem.Write((U8)4);
		}
		if (!mesh.vertexUvset_0.empty())
		{
			outMem.Write(Mesh::AttributeSemantic::TEXCOORD0);
			outMem.Write(Mesh::AttributeType::F32);
			outMem.Write((U8)2);
		}

		// Indices
		const bool are16bit = AreIndices16Bit(mesh);
		if (are16bit)
		{
			I32 indexSize = sizeof(U16);
			outMem.Write(indexSize);
			outMem.Write((I32)mesh.indices.size());
			for (U32 i : mesh.indices)
			{
				ASSERT(i <= (1 << 16));
				U16 index = (U16)i;
				outMem.Write(index);
			}
		}
		else
		{
			I32 indexSize = sizeof(U32);
			outMem.Write(indexSize);
			outMem.Write((I32)mesh.indices.size());
			outMem.Write(&mesh.indices[0], sizeof(U32) * mesh.indices.size());
		}

		// Vertex datas
		if (!mesh.vertexPositions.empty())
		{
			outMem.Write(mesh.vertexPositions.size());
			outMem.Write(mesh.vertexPositions.data(), mesh.vertexPositions.size() * sizeof(F32x3));
		}
		if (!mesh.vertexNormals.empty())
		{
			outMem.Write(mesh.vertexNormals.size());
			outMem.Write(mesh.vertexNormals.data(), mesh.vertexNormals.size() * sizeof(F32x3));
		}
		if (!mesh.vertexTangents.empty())
		{
			outMem.Write(mesh.vertexTangents.size());
			outMem.Write(mesh.vertexTangents.data(), mesh.vertexTangents.size() * sizeof(F32x4));
		}
		if (!mesh.vertexUvset_0.empty())
		{
			outMem.Write(mesh.vertexUvset_0.size());
			outMem.Write(mesh.vertexUvset_0.data(), mesh.vertexUvset_0.size() * sizeof(F32x2));
		}

		return true;
	}
}
}