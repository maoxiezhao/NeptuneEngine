#include "importModel.h"
#include "modelTool.h"
#include "editor\editor.h"

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

	bool ImportModelDataObj(const char* path, ModelImporter::ImportModel& modelData, const ModelImporter::ImportConfig& cfg)
	{
		PROFILE_FUNCTION();

		tinyobj::attrib_t objAttrib;
		std::vector<tinyobj::shape_t> objShapes;
		std::vector<tinyobj::material_t> objMaterials;

		FileSystem& fs = Engine::Instance->GetFileSystem();
		OutputMemoryStream mem;
		if (!fs.LoadContext(path, mem))
			return false;

		char srcDir[MAX_PATH_LENGTH];
		memset(srcDir, 0, sizeof(srcDir));
		CopyString(Span(srcDir), Path::GetDir(path));

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

		Array<tinyobj::mesh_t*> objMeshes;

		// Gather meshes
		auto& meshes = modelData.meshes;
		auto& lods = modelData.lods;
		meshes.reserve(objShapes.size());
		for (auto& shape : objShapes)
		{
			auto& mesh = meshes.emplace();
			mesh.name = shape.name;

			objMeshes.push_back(&shape.mesh);

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

		// Collect mesh datas
		for (int meshIndex = 0; meshIndex < meshes.size(); meshIndex++)
		{
			auto& importMesh = meshes[meshIndex];
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
						objMeshes[meshIndex]->indices[subset.indexOffset + i + 0],
						objMeshes[meshIndex]->indices[subset.indexOffset + i + 1],
						objMeshes[meshIndex]->indices[subset.indexOffset + i + 2],
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

						static const bool transformToLH = true;
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

		// Gather materials
		auto& materials = modelData.materials;
		for (auto& objMaterial : objMaterials)
		{
			ModelImporter::ImportMaterial& mat = materials.emplace();
			mat.name = objMaterial.name;
			mat.color = F32x4(objMaterial.diffuse[0], objMaterial.diffuse[1], objMaterial.diffuse[2], 1.0f);
			mat.metallic = objMaterial.metallic;
			mat.roughness = objMaterial.roughness;

			auto GatherTexture = [&mat, &fs, srcDir, &objMaterial, &modelData](Texture::TextureType type) {

				std::string texname;
				switch (type)
				{
				case Texture::DIFFUSE:
					texname = objMaterial.diffuse_texname;
					break;
				case Texture::NORMAL:
					texname = objMaterial.normal_texname;
					if (texname.empty())
						texname = objMaterial.bump_texname;
					break;
				case Texture::SPECULAR:
					texname = objMaterial.specular_texname;
					break;
				default:
					break;
				}
				if (texname.empty())
					return;

				ModelImporter::ImportTexture& tex = mat.textures[(U32)type];
				tex.type = type;
				tex.path = Path::Join(Span(srcDir), Span(texname.c_str(), texname.size()));
				tex.isValid = fs.FileExists(tex.path.c_str());
				tex.import = true;

				modelData.textures.push_back(tex);
			};

			GatherTexture(Texture::TextureType::DIFFUSE);
			GatherTexture(Texture::TextureType::NORMAL);
			GatherTexture(Texture::TextureType::SPECULAR);
		}

		return true;
	}
}
}