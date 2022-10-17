#include "modelImporter.h"
#include "modelTool.h"
#include "importModel.h"
#include "level\level.h"
#include "core\utils\deleteHandler.h"
#include "editor\editor.h"
#include "contentImporters\texture\textureImporter.h"
#include "contentImporters\material\createMaterial.h"
#include "contentImporters\resourceImportingManager.h"

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


	bool ImportModelData(const String& path, ModelImporter::ImportModel& modelData, ModelImporter::ImportConfig& cfg, const Path& importOutput)
	{
		Logger::Info("Importing model from %s", path.c_str());
		const auto startTime = Timer::GetTimeSeconds();

		// Import model data
		char ext[5] = {};
		CopyString(Span(ext), Path::GetExtension(path.toSpan()));
		MakeLowercase(Span(ext), ext);
		if (EqualString(ext, "obj"))
		{
			if (!ImportModelDataOBJ(path.c_str(), modelData, cfg))
				return false;
		}
		else if (EqualString(ext, "gltf") || EqualString(ext, "glb"))
		{
			if (!ImportModelDataGLTF(path.c_str(), modelData, cfg))
				return false;

			cfg.type = ModelImporter::ModelType::Scene;
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
			if (importOutput.IsEmpty() || !tex.isValid)
				continue;

			auto texFilename = Path::GetBaseName(tex.path);
			auto filename = Path(texFilename.data());
			if (importedFiles.contains(filename))
			{
				I32 counter = 1;
				do
				{
					filename = StaticString<MAX_PATH_LENGTH>().Sprintf("%s_%d", texFilename, counter);
					counter++;
				} while (importedFiles.contains(filename));
			}
			importedFiles.insert(filename, true);

			auto texPath = Path(tex.path);
			auto outputPath = importOutput / tex.path + RESOURCE_FILES_EXTENSION_WITH_DOT;
			TextureImporter::ImportConfig config = {};
			ResourceImportingManager::ImportIfEdited(texPath, outputPath, tex.guid, &config);
		}

		// Perpare materials
		for (auto& material : modelData.materials)
		{
			if (!material.import)
				continue;

			CreateMaterial::Options options = {};
			memset(&options.info, 0, sizeof(MaterialInfo));
			options.info.domain = MaterialDomain::Object;
			options.info.type = MaterialType::Standard;
			options.info.side = material.doubleSided ? ObjectDoubleSided::OBJECT_DOUBLESIDED_FRONTSIDE : ObjectDoubleSided::OBJECT_DOUBLESIDED_FRONTSIDE;
			options.info.blendMode = material.blendMode;

			// BaseColor
			options.baseColor = Color4(material.color);
			options.metalness = material.metallic;
			options.roughness = material.roughness;
			options.alphaRef = material.alphaRef;

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

			auto matPath = importOutput / material.name + RESOURCE_FILES_EXTENSION_WITH_DOT;
			if (!ResourceImportingManager::Create(
				ResourceImportingManager::CreateMaterialTag,
				material.guid,
				matPath,
				&options))
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
		IMPORT_SETUP(Model);

		ImportConfig cfg;
		if (ctx.customArg != nullptr)
			cfg = *static_cast<ImportConfig*>(ctx.customArg);

		const PathInfo pathInfo(ctx.input);
		Path importOutputPath(StaticString<MAX_PATH_LENGTH>(pathInfo.dir, "/", pathInfo.basename, "/").c_str());

		// Import model data
		ImportModel importModelData;
		if (!ImportModelData(ctx.input, importModelData, cfg, importOutputPath))
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
		case ModelType::Scene:
			ret = WriteScene(ctx, importModelData, importOutputPath);
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

	bool ModelImporter::WriteSceneNode(WriteSceneContext& ctx, const ImportNode& node, ECS::Entity parent, const Path& importOutput)
	{
		auto entity = ctx.scene->CreateEntity(node.name.c_str());
		if (parent != ECS::INVALID_ENTITY)
			entity.ChildOf(parent);

		// Transform
		entity.Add<TransformComponent>();
		auto transform = entity.GetMut<TransformComponent>();
		if (transform != nullptr)
		{
			transform->transform = node.transform;
		}

		// Mesh
		if (node.meshIndex >= 0)
		{
			auto& mesh = ctx.modelData.meshes[node.meshIndex];
			if (mesh.import && !mesh.indices.empty())
			{
				entity.Add<ObjectComponent>()
					.Add<MeshComponent>()
					.Add<MaterialComponent>();

				ObjectComponent* obj = entity.GetMut<ObjectComponent>();
				obj->mesh = entity;		

				// Write mesh as model resource
				ImportModel modelData;
				auto& lod = modelData.lods.emplace();
				lod.meshes.push_back(&mesh);

				// TEMP
				const I32 meshLods = 1;
				MeshComponent* meshComp = entity.GetMut<MeshComponent>();
				meshComp->meshCount = 1;
				meshComp->lodsCount = meshLods;
				for (int lodIndex = 0; lodIndex < meshLods; lodIndex++)
				{
					auto& meshInfo = meshComp->meshes[lodIndex].emplace();
					meshInfo.aabb = mesh.aabb;
					meshInfo.meshIndex = 0;
					
					// Write scene materials
					meshInfo.material = entity;		
					MaterialComponent* comp = entity.GetMut<MaterialComponent>();
					comp->materials.resize(mesh.subsets.size());
					for (I32 i = 0; i < mesh.subsets.size(); i++)
					{
						auto& material = ctx.modelData.materials[mesh.subsets[i].materialIndex];
						mesh.subsets[i].materialIndex = i;
						comp->materials[i].SetVirtualID(material.guid);

						modelData.materials.push_back(material);
					}
				}
	
				Guid modelResID = Guid::New();

				auto outputPath = importOutput / mesh.name.c_str() + RESOURCE_FILES_EXTENSION_WITH_DOT;
				if (!ResourceImportingManager::Create(
					ResourceImportingManager::CreateModelTag,
					modelResID,
					outputPath,
					&modelData))
				{
					Logger::Warning("Faield to create a mesh %s from model %s", mesh.name.c_str(), ctx.ctx.input.c_str());
					return false;
				}

				meshComp->model.SetVirtualID(modelResID);
			}
		}

		bool ret = true;
		for (const auto& child : node.children)
		{
			if (!WriteSceneNode(ctx, child, entity, importOutput))
			{
				ret = false;
				break;
			}
		}
		return ret;
	}

	CreateResult ModelImporter::WriteScene(CreateResourceContext& ctx, ImportModel& modelData, const Path& importOutput)
	{
		// ScriptingObjectParams params(ctx.initData.header.guid);
		auto scene = CJING_NEW(Scene)();
		DeleteHandler handler(scene);
		scene->SetName(ctx.input.c_str());

		// Write scene nodes
		WriteSceneContext writeCtx(ctx, modelData, scene);
		if (!WriteSceneNode(writeCtx, modelData.root, ECS::INVALID_ENTITY, importOutput))
		{
			Logger::Error("Failed to writer scene nodes %s", ctx.input.c_str());
			return CreateResult::Error;
		}

		PathInfo pathInfo(ctx.input);
		StaticString<MAX_PATH_LENGTH> scenePath(pathInfo.dir, pathInfo.basename, ".scene");
		rapidjson_flax::StringBuffer outData;
		if (!Level::SaveScene(scene, outData))
		{
			Logger::Warning("Failed to save scene %s", scenePath.c_str());
			return CreateResult::Error;
		}

		auto file = FileSystem::OpenFile(scenePath.c_str(), FileFlags::DEFAULT_WRITE);
		if (!file->IsValid())
		{
			Logger::Error("Failed to create the scene resource file");
			return CreateResult::Error;
		}
		file->Write(outData.GetString(), outData.GetSize());
		file->Close();

		// Register scene resource
		ResourceManager::GetCache().Register(scene->GetGUID(), SceneResource::ResType, Path(scenePath));

		ctx.output = scenePath;
		ctx.initData.header.type = SceneResource::ResType;
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

	CreateResult ModelImporter::Create(CreateResourceContext& ctx)
	{
		ASSERT(ctx.customArg != nullptr);
		auto& modelData = *(ImportModel*)ctx.customArg;

		if (modelData.lods.empty() || modelData.lods[0].meshes.empty())
		{
			Logger::Warning("Models has no valid meshes");
			return CreateResult::Error;
		}

		return WriteModel(ctx, modelData);
	}
}
}