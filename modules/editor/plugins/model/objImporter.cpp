#include "objImporter.h"
#include "modelTool.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"
#include "editor\plugins\material\createMaterial.h"
#include "core\filesystem\filesystem.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "loader\tiny_obj_loader.h"

namespace VulkanTest
{
namespace Editor
{
	struct membuf : std::streambuf
	{
		membuf(char* begin, char* end) {
			this->setg(begin, begin, end);
		}
	};

	// Custom material file reader:
	class MaterialFileReader : public tinyobj::MaterialReader 
	{
	public:
		explicit MaterialFileReader(FileSystem& fs_, const std::string& basedir_)
			: basedir(basedir_), fs(fs_) {}

		virtual bool operator()(
			const std::string& matId,
			std::vector<tinyobj::material_t>* materials,
			std::map<std::string, int>* matMap, 
			std::string* err)
		{
			std::string filepath;
			if (!basedir.empty()) {
				filepath = std::string(basedir) + matId;
			}
			else {
				filepath = matId;
			}

			OutputMemoryStream mem;
			if (!fs.LoadContext(filepath.c_str(), mem))
			{
				std::string ss;
				ss += "WARN: Material file [ " + filepath + " ] not found.\n";
				if (err) {
					(*err) += ss;
				}
				return false;
			}
			membuf sbuf((char*)mem.Data(), (char*)mem.Data() + mem.Size());
			std::istream matIStream(&sbuf);

			std::string warning;
			LoadMtl(matMap, materials, &matIStream, &warning);

			if (!warning.empty()) {
				if (err) {
					(*err) += warning;
				}
			}

			return true;
		}

	private:
		FileSystem& fs;
		std::string basedir;
	};

	OBJImporter::OBJImporter(EditorApp& editor_) :
		editor(editor_)
	{
	}

	OBJImporter::~OBJImporter()
	{
	}

	bool OBJImporter::ImportModel(const char* filename)
	{
		PROFILE_FUNCTION();

		objShapes.clear();
		objMaterials.clear();

		FileSystem& fs = editor.GetEngine().GetFileSystem();
		OutputMemoryStream mem;
		if (!fs.LoadContext(filename, mem))
			return false;

		char srcDir[MAX_PATH_LENGTH];
		CopyString(Span(srcDir), Path::GetDir(filename));

		std::string objErrors;
		membuf sbuf((char*)mem.Data(), (char*)mem.Data() + mem.Size());
		std::istream in(&sbuf);
		MaterialFileReader matFileReader(fs, srcDir);
		if (!tinyobj::LoadObj(&objAttrib, &objShapes, &objMaterials, &objErrors, &in, &matFileReader, true))
		{
			if (!objErrors.empty())
				Logger::Error(objErrors.c_str());
			return false;
		}
	
		// Gather meshes
		for (auto& shape : objShapes)
		{
			auto& mesh = meshes.emplace();
			mesh.mesh = &shape.mesh;
			mesh.name = shape.name;

			// Detect mesh subsets
			std::unordered_map<int, int> materialHash = {};
			for (size_t i = 0; i < shape.mesh.indices.size(); i += 3)
			{
				tinyobj::index_t reorderedIndices[] = {
					shape.mesh.indices[i + 0],
					shape.mesh.indices[i + 1],
					shape.mesh.indices[i + 2],
				};
				for (auto& index : reorderedIndices)
				{
					int materialIndex = std::max(0, shape.mesh.material_ids[i / 3]); // this indexes the material library
					if (materialHash.count(materialIndex) == 0)
					{
						materialHash[materialIndex] = 1;
						
						auto& subset = mesh.subsets.emplace();
						subset.materialIndex = materialIndex;
						subset.indexOffset = i;
					}

					mesh.subsets.back().indexCount++;
				}
			}

			// Detect mesh lod
			mesh.lod = ModelTool::DetectLodIndex(mesh.name.c_str());

			if (lods.size() <= mesh.lod)
				lods.resize(mesh.lod + 1);
			lods[mesh.lod].meshes.push_back(&mesh);
		}

		// Gather materials
		for (auto& objMaterial : objMaterials)
		{
			ImportMaterial& mat = materials.emplace();
			mat.material = &objMaterial;

			auto GatherTexture = [this, &mat, &fs, srcDir](Texture::TextureType type) {

				std::string texname;
				switch (type)
				{
				case Texture::DIFFUSE:
					texname = mat.material->diffuse_texname;
					break;
				case Texture::NORMAL:
					texname = mat.material->normal_texname;
					break;
				case Texture::SPECULAR:
					texname = mat.material->specular_texname;
					break;
				default:
					break;
				}
				if (texname.empty())
					return;

				ImportTexture& tex = mat.textures[(U32)type];
				tex.type = type;
				tex.path = Path::Join(Span(srcDir), Span(texname.c_str(), texname.size()));
				tex.isValid = fs.FileExists(tex.path.c_str());
				tex.import = true;
			};

			GatherTexture(Texture::TextureType::DIFFUSE);
			GatherTexture(Texture::TextureType::NORMAL);
			GatherTexture(Texture::TextureType::SPECULAR);
		}

		return true;
	}

	static const bool transformToLH = true;

	void OBJImporter::PostprocessMeshes(const ImportConfig& cfg)
	{
		// Collect mesh datas
		for (auto& importMesh : meshes)
		{
			importMesh.vertexPositions.clear();
			importMesh.vertexNormals.clear();
			importMesh.vertexTangents.clear();
			importMesh.vertexUvset_0.clear();
			importMesh.indices.clear();

			auto& aabb = importMesh.aabb;
			std::unordered_map<size_t, uint32_t> uniqueVertices = {};
			for (auto& subset : importMesh.subsets)
			{
				subset.uniqueIndexOffset = importMesh.indices.size();

				for (size_t i = 0; i < subset.indexCount; i += 3)
				{
					tinyobj::index_t reorderedIndices[] = {
						importMesh.mesh->indices[subset.indexOffset + i + 0],
						importMesh.mesh->indices[subset.indexOffset + i + 1],
						importMesh.mesh->indices[subset.indexOffset + i + 2],
					};
					for (auto& index : reorderedIndices)
					{
						F32x3 pos = F32x3(
							objAttrib.vertices[index.vertex_index * 3 + 0],
							objAttrib.vertices[index.vertex_index * 3 + 1],
							objAttrib.vertices[index.vertex_index * 3 + 2]
						);

						F32x3 nor = F32x3(0, 0, 0);
						if (!objAttrib.normals.empty())
						{
							nor = F32x3(
								objAttrib.normals[index.normal_index * 3 + 0],
								objAttrib.normals[index.normal_index * 3 + 1],
								objAttrib.normals[index.normal_index * 3 + 2]
							);
						}

						F32x2 tex = F32x2(0, 0);
						if (index.texcoord_index >= 0 && !objAttrib.texcoords.empty())
						{
							tex = F32x2(
								objAttrib.texcoords[index.texcoord_index * 2 + 0],
								1 - objAttrib.texcoords[index.texcoord_index * 2 + 1]
							);
							importMesh.hasUV = true;
						}

						if (transformToLH)
						{
							pos.z *= -1;
							nor.z *= -1;
						}

						HashCombiner hash;
						hash.HashCombine(index.vertex_index);
						hash.HashCombine(index.normal_index);
						hash.HashCombine(index.texcoord_index);

						auto vertexHash = hash.Get();
						if (uniqueVertices.count(vertexHash) == 0)
						{
							uniqueVertices[vertexHash] = (uint32_t)importMesh.vertexPositions.size();
							importMesh.vertexPositions.push_back(pos);
							importMesh.vertexNormals.push_back(nor);
							importMesh.vertexUvset_0.push_back(tex);
						}
						importMesh.indices.push_back(uniqueVertices[vertexHash]);
						subset.uniqueIndexCount++;

						aabb.min = Min(aabb.min, pos);
						aabb.max = Max(aabb.max, pos);
					}
				}
			}
		}

		// Calculate vertex tangents
		for (auto& importMesh : meshes)
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

	void OBJImporter::GetImportMeshName(const ImportMesh& mesh, char(&out)[256])
	{
		CopyString(out, mesh.name.c_str());
	}

	void OBJImporter::WriteMesh(OutputMemoryStream& outmem, const ImportMesh& mesh)
	{
		// Mesh format:
		// ----------------------------------
		// SubsetCount
		// Subset1
		// Subset2
		//   MatPath
		//   indexOffset
		//   indexCount
		// AttrCount
		// Attr1 (Semantic, Type, Count)
		// Attr2 (Semantic, Type, Count)
		// Indiecs
		// VertexDatas

		// Mesh subsets		
		outmem.Write(mesh.subsets.size());
		for (const auto& subset : mesh.subsets)
		{
			outmem.Write(subset.materialIndex);
			outmem.Write(subset.uniqueIndexOffset);
			outmem.Write(subset.uniqueIndexCount);
		}

		// Attributes (pos, normal, texcoord)
		U32 attrCount = GetAttributeCount(mesh);
		outmem.Write(attrCount);
		outmem.Write(Mesh::AttributeSemantic::POSITION);
		outmem.Write(Mesh::AttributeType::F32);
		outmem.Write((U8)3);

		outmem.Write(Mesh::AttributeSemantic::NORMAL);
		outmem.Write(Mesh::AttributeType::F32);
		outmem.Write((U8)3);

		outmem.Write(Mesh::AttributeSemantic::TANGENT);
		outmem.Write(Mesh::AttributeType::F32);
		outmem.Write((U8)4);

		outmem.Write(Mesh::AttributeSemantic::TEXCOORD0);
		outmem.Write(Mesh::AttributeType::F32);
		outmem.Write((U8)2);

		// Indices
		const bool are16bit = AreIndices16Bit(mesh);
		if (are16bit)
		{
			I32 indexSize = sizeof(U16);
			outmem.Write(indexSize);
			outmem.Write((I32)mesh.indices.size());
			for (U32 i : mesh.indices)
			{
				ASSERT(i <= (1 << 16));
				U16 index = (U16)i;
				outmem.Write(index);
			}
		}
		else
		{
			I32 indexSize = sizeof(U32);
			outmem.Write(indexSize);
			outmem.Write((I32)mesh.indices.size());
			outmem.Write(&mesh.indices[0], sizeof(U32) * mesh.indices.size());
		}

		// Vertex datas
		outmem.Write(mesh.vertexPositions.size());
		outmem.Write(mesh.vertexPositions.data(), mesh.vertexPositions.size() * sizeof(F32x3));
		outmem.Write(mesh.vertexNormals.data(), mesh.vertexNormals.size() * sizeof(F32x3));
		outmem.Write(mesh.vertexUvset_0.data(), mesh.vertexUvset_0.size() * sizeof(F32x2));
	}

	bool OBJImporter::AreIndices16Bit(const ImportMesh& mesh) const
	{
		// TODO: mesh only support U32 now..
		// return mesh.indices.size() < (1 << 16);
		return false;
	}

	I32 OBJImporter::GetAttributeCount(const ImportMesh& mesh) const
	{
		return 4; // Pos & Normals & Tagents & UV
	}

	bool OBJImporter::WriteModel(const char* filepath, const ImportConfig& cfg)
	{
		PROFILE_FUNCTION();
		if (meshes.empty())
			return false;
		
		// Postprocess meshes (Auto lods/Calculate Tagents)
		PostprocessMeshes(cfg);

		const PathInfo pathInfo(filepath);

		// Create importing materials
		if (!WriteMaterials(filepath, cfg))
			return false;

		// Write compiled model
		ResourceDataWriter writer(Model::ResType);

		// Model header
		Model::FileHeader header;
		header.magic = Model::FILE_MAGIC;
		header.version = Model::FILE_VERSION;
		writer.WriteCustomData(header);

		// Write base model data in chunk 0
		auto dataChunk = writer.GetChunk(0);
		auto outMem = &dataChunk->mem;

		// Write mateirals
		outMem->Write((I32)materials.size());
		for (const auto& material : materials)
		{
			StaticString<MAX_PATH_LENGTH + 128> matPath(pathInfo.dir, material.material->name.c_str(), ".mat");
			const I32 len = StringLength(matPath.c_str());
			outMem->Write(len);
			outMem->Write(matPath.c_str(), len);
		
			// TODO Use GUID to replace Path
		}

		// Write lods
		I32 lodsCount = lods.size();
		outMem->Write((U8)lodsCount);
		for (int i = 0; i < lodsCount; i++)
		{
			auto& lod = lods[i];
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
			}
		}

		// Write lod chunk datas
		for (int i = 0; i < lods.size(); i++)
		{
			auto lodDataChunk = writer.GetChunk(MODEL_LOD_TO_CHUNK_INDEX(i));
			auto lodMem = &lodDataChunk->mem;
			for (auto& mesh : lods[i].meshes)
				WriteMesh(*lodMem, *mesh);
		}

		// Write compiled data
		return editor.GetAssetCompiler().WriteCompiled(filepath, writer.data);
	}

	bool OBJImporter::WriteMaterials(const char* filepath, const ImportConfig& cfg)
	{
		PROFILE_FUNCTION();

		FileSystem& fs = editor.GetEngine().GetFileSystem();
		const PathInfo pathInfo(filepath);

		for (const auto& material : materials)
		{
			if (!material.import)
				continue;

			CreateMaterial::Options options = {};
			memset(&options.info, 0, sizeof(MaterialInfo));
			options.info.domain = MaterialDomain::Object;
			options.info.type = MaterialType::Standard;

			// BaseColor
			options.baseColor = Color4(F32x4(material.material->diffuse[0], material.material->diffuse[1], material.material->diffuse[2], 1.0f));

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

			const auto& matName = material.material->name;
			StaticString<MAX_PATH_LENGTH> matPath(pathInfo.dir, matName, ".mat");
			if (!CreateMaterial::Create(editor, material.guid, Path(matPath), options))
			{
				Logger::Warning("Faield to create a material %s from model %s", matPath.c_str(), filepath);
				return false;
			}
		}

		return true;
	}
}
}