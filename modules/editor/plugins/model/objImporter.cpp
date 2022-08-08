#include "objImporter.h"
#include "editor\editor.h"
#include "editor\widgets\assetCompiler.h"
#include "core\filesystem\filesystem.h"
#include "renderer\model.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "loader\tiny_obj_loader.h"

namespace VulkanTest
{
namespace Editor
{
	struct membuf : std::streambuf
	{
		membuf(char* begin, char* end) 
		{
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

	bool OBJImporter::Import(const char* filename)
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
	
		for (auto& shape : objShapes)
		{
			// Gather meshes
			auto& mesh = meshes.emplace();
			mesh.mesh = &shape.mesh;
			mesh.name = shape.name;

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
						subset.material = &objMaterials[materialIndex];
						subset.indexOffset = i;
					}

					mesh.subsets.back().indexCount++;
				}
			}
		}

		// Gather materials
		for (auto& mesh : meshes)
		{
			for (auto& subset : mesh.subsets)
			{
				if (subset.material == nullptr)
					continue;

				ImportMaterial& mat = materials.emplace();
				mat.material = subset.material;

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
		}

		return true;
	}

	static const bool transformToLH = true;

	

	void OBJImporter::PostprocessMeshes(const ImportConfig& cfg)
	{
		for (auto& importMesh : meshes)
		{
			importMesh.vertexPositions.clear();
			importMesh.vertexNormals.clear();
			importMesh.vertexTangents.clear();
			importMesh.vertexUvset_0.clear();
			importMesh.indices.clear();

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
					}
				}
			}
		}
	}

	void OBJImporter::GetImportMeshName(const ImportMesh& mesh, char(&out)[256])
	{
		CopyString(out, mesh.name.c_str());
	}

	void OBJImporter::WriteHeader()
	{
		Model::FileHeader header;
		header.magic = Model::FILE_MAGIC;
		header.version = Model::FILE_VERSION;
		outMem.Write(&header, sizeof(header));
	}

	void OBJImporter::WriteMesh(const char* src, const ImportMesh& mesh)
	{
		// Mesh format:
		// ----------------------------------
		// Mesh name
		// AttrCount
		// Attr1 (Semantic, Type, Count)
		// Attr2 (Semantic, Type, Count)
		// SubsetCount
		// Subset1
		// Subset2
		//   MatPath
		//   indexOffset
		//   indexCount

		const PathInfo srcInfo(src);

		// Mesh name
		char name[256];
		GetImportMeshName(mesh, name);
		I32 nameLen = (I32)StringLength(name);
		Write(nameLen);
		Write(name, nameLen);

		// Attributes (pos, normal, texcoord)
		U32 attrCount = GetAttributeCount(mesh);
		Write(attrCount);
		Write(Mesh::AttributeSemantic::POSITION);
		Write(Mesh::AttributeType::F32);
		Write((U8)3);

		Write(Mesh::AttributeSemantic::NORMAL);
		Write(Mesh::AttributeType::F32);
		Write((U8)3);

		Write(Mesh::AttributeSemantic::TEXCOORD0);
		Write(Mesh::AttributeType::F32);
		Write((U8)2);

		// Material path		
		Write(mesh.subsets.size());
		for(const auto& subset : mesh.subsets)
		{
			StaticString<MAX_PATH_LENGTH + 128> matPath(srcInfo.dir, subset.material->name.c_str(), ".mat");
			const I32 len = StringLength(matPath.c_str());
			Write(len);
			Write(matPath.c_str(), len);

			Write(subset.uniqueIndexOffset);
			Write(subset.uniqueIndexCount);
		}
	}

	void OBJImporter::WriteMeshes(const char* src, I32 meshIdx, const ImportConfig& cfg)
	{
		// Meshes format:
		// -------------------------
		// MeshCount
		// Mesh1
		// Mesh2

		// Write mesh count
		I32 meshCount = 0;
		if (meshIdx <= 0) 
		{
			for (auto& mesh : meshes) 
			{
				if (mesh.import && mesh.lod == 0)
					meshCount++;
			}
		}
		else 
		{
			meshCount = 1;
		}
		Write(meshCount);

		// Write meshes
		if (meshIdx <= 0) 
		{
			for (auto& mesh : meshes)
			{
				if (mesh.import && mesh.lod == 0)
					WriteMesh(src, mesh);
			}
		}
		else 
		{
			WriteMesh(src, meshes[meshIdx]);
		}
	}

	void OBJImporter::WriteGeometry(const ImportConfig& cfg)
	{
		// Indices
		// VertexData
		// -- Count
		// -- Vertex
		// -- Normals
		// -- Texcoords

		// Write indices
		for (const auto& importMesh : meshes)
		{
			const bool are16bit = AreIndices16Bit(importMesh);
			if (are16bit)
			{
				I32 indexSize = sizeof(U16);
				Write(indexSize);
				Write((I32)importMesh.indices.size());
				for (U32 i : importMesh.indices)
				{
					ASSERT(i <= (1 << 16));
					U16 index = (U16)i;
					Write(index);
				}
			}
			else
			{
				I32 indexSize = sizeof(U32);
				Write(indexSize);
				Write((I32)importMesh.indices.size());
				Write(&importMesh.indices[0], sizeof(U32) * importMesh.indices.size());
			}
		}

		// Write vertex data
		for (const auto& importMesh : meshes)
		{
			Write(importMesh.vertexPositions.size());
			Write(importMesh.vertexPositions.data(), importMesh.vertexPositions.size() * sizeof(F32x3));
			Write(importMesh.vertexNormals.data(), importMesh.vertexNormals.size() * sizeof(F32x3));
			Write(importMesh.vertexUvset_0.data(), importMesh.vertexUvset_0.size() * sizeof(F32x2));
		}
	}

	bool OBJImporter::AreIndices16Bit(const ImportMesh& mesh) const
	{
		// TODO: mesh only support U32 now..
		// return mesh.indices.size() < (1 << 16);
		return false;
	}

	I32 OBJImporter::GetAttributeCount(const ImportMesh& mesh) const
	{
		return 3; // Pos & Normals & UV
	}

	void OBJImporter::WriteString(const char* str)
	{
		outMem.Write(str, StringLength(str));
	}

	void OBJImporter::WriteModel(const char* filepath, const ImportConfig& cfg)
	{
		PROFILE_FUNCTION();
		PostprocessMeshes(cfg);

		if (meshes.empty())
			return;

		outMem.Clear();

		// Model format:
		// ---------------------------
		// Model header
		// Meshes
		// Geometry

		WriteHeader();
		WriteMeshes(filepath, -1, cfg);
		WriteGeometry(cfg);

		editor.GetAssetCompiler().WriteCompiled(filepath, Span(outMem.Data(), outMem.Size()));
	}

	void OBJImporter::WriteMaterials(const char* filepath, const ImportConfig& cfg)
	{
		PROFILE_FUNCTION();
	
		FileSystem& fs = editor.GetEngine().GetFileSystem();
		const PathInfo pathInfo(filepath);
		for (const auto& material : materials)
		{
			if (!material.import)
				continue;

			const auto& matName = material.material->name;
			StaticString<MAX_PATH_LENGTH> matPath(pathInfo.dir, matName, ".mat");
			if (fs.FileExists(matPath.c_str()))
				continue;

			auto file = fs.OpenFile(matPath.c_str(), FileFlags::DEFAULT_WRITE);
			if (!file)
			{
				Logger::Error("Failed to create mat file %s", matPath.c_str());
				continue;
			}

			outMem.Clear();

			// TODO: Use MaterialImporter to write material
			// Write texture header
			MaterialInfo materialInfo;
			memset(&materialInfo, 0, sizeof(MaterialInfo));
			materialInfo.domain = MaterialDomain::Object;
			materialInfo.useCustomShader = false;
			outMem.Write(materialInfo);

			// Write params
			auto WriteTexture = [this, material](Texture::TextureType type)
			{
				auto& texture = material.textures[(U32)type];
				if (texture.isValid && !texture.path.empty())
				{
					WriteString("texture \"/");
					WriteString(texture.path.c_str());
					WriteString("\"\n");
				}
				else
				{
					WriteString("texture \"\"\n");
				}
			};
			
			WriteTexture(Texture::TextureType::DIFFUSE);
			WriteTexture(Texture::TextureType::NORMAL);
			WriteTexture(Texture::TextureType::SPECULAR);

			auto diffuseColor = material.material->diffuse;
			outMem << "color {" 
				<< diffuseColor[0] << ","
				<< diffuseColor[1] << ","
				<< diffuseColor[2] << ",1}\n";

			if (!file->Write(outMem.Data(), outMem.Size()))
				Logger::Error("Failed to write mat file %s", matPath.c_str());
			
			file->Close();
		}
	}
}
}